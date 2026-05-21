import os
import torch
os.environ["OMP_NUM_THREADS"] = "1"
os.environ["OPENBLAS_NUM_THREADS"] = "1"
os.environ["MKL_NUM_THREADS"] = "1"
os.environ["VECLIB_MAXIMUM_THREADS"] = "1"
os.environ["NUMEXPR_NUM_THREADS"] = "1"
torch.set_num_threads(1)

import cv2
import queue
import threading
import argparse
import time
from ultralytics import YOLO


def worker(input_queue, output_queue):

    # Инициализируем модель внутри потока
    model = YOLO("yolov8s-pose.pt")

    while True:
        item = input_queue.get()
        if item is None:
            break

        frame_idx, frame = item

        # Предсказание через YOLO (pose)
        results = model(frame, verbose=False)

        # Получаем кадр с нанесенными точками
        annotated_frame = results[0].plot()

        output_queue.put((frame_idx, annotated_frame))
        input_queue.task_done()


def writer(output_queue, output_video, is_realtime):

    next_frame_idx = 0
    buffer = {}

    if is_realtime:
        cv2.namedWindow("Real-Time Pose", cv2.WINDOW_NORMAL)

    while True:
        item = output_queue.get()
        if item is None:
            break

        frame_idx, frame = item
        buffer[frame_idx] = frame

        while next_frame_idx in buffer:
            out_frame = buffer.pop(next_frame_idx)
            output_video.write(out_frame)

            if is_realtime:
                cv2.imshow("Real-Time Pose", out_frame)
                cv2.waitKey(1)

            next_frame_idx += 1

    if is_realtime:
        cv2.destroyAllWindows()


def main():
    parser = argparse.ArgumentParser(
        description="Многопоточная обработка видео (нанесение Keypoints)")
    parser.add_argument('--input', type=str, default='0',
                        help='Входной файл видео или номер камеры (по умолчанию: 0)')
    parser.add_argument('--output', type=str,
                        default='output.mp4', help='Выходной файл')
    parser.add_argument('--threads', type=int, default=4,
                        help='Количество потоков для обработки')
    parser.add_argument('--frames', type=int, default=150,
                        help='Количество кадров для чтения')
    parser.add_argument('--realtime', action='store_true',
                        help='Включить режим реального времени (вывод на экран и отбрасывание кадров при отставании)')

    args = parser.parse_args()

    if args.input.isdigit():
        cap = cv2.VideoCapture(int(args.input))
    else:
        cap = cv2.VideoCapture(args.input)

    if not cap.isOpened():
        print(f"Ошибка: Не удалось открыть {args.input}")
        return

    width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    fps = cap.get(cv2.CAP_PROP_FPS)
    if not fps or fps == 0:
        fps = 30.0

    total_frames_in_file = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    frames_to_process = args.frames if (
        total_frames_in_file <= 0 or args.input.isdigit()) else total_frames_in_file
    frames_to_process = min(
        frames_to_process, args.frames) if args.frames > 0 else frames_to_process

    fourcc = cv2.VideoWriter_fourcc(*'mp4v')
    out = cv2.VideoWriter(args.output, fourcc, fps, (width, height))

    # Ограничиваем размер очереди, чтобы не забивать оперативную память
    input_queue = queue.Queue(maxsize=args.threads * 4)
    output_queue = queue.Queue()

    # 1. Запуск потоков обработки
    threads = []
    for _ in range(args.threads):
        t = threading.Thread(target=worker, args=(input_queue, output_queue))
        t.start()
        threads.append(t)

    # Запуск потока записи
    writer_thread = threading.Thread(
        target=writer, args=(output_queue, out, args.realtime))
    writer_thread.start()

    print(f"Начало обработки. Потоков: {args.threads}")
    start_time = time.time()

    # 2. Читаем кадры и кидаем во входной буфер
    frame_idx = 0
    while frame_idx < frames_to_process:
        ret, frame = cap.read()
        if not ret:
            break

        if args.realtime:
            try:
                input_queue.put_nowait((frame_idx, frame))
                frame_idx += 1
            except queue.Full:
                pass
        else:
            input_queue.put((frame_idx, frame))
            frame_idx += 1

    # Ждем, пока потоки обработают все кадры во входной очереди
    input_queue.join()

    # Отправляем сигналы остановки worker-ам
    for _ in range(args.threads):
        input_queue.put(None)
    for t in threads:
        t.join()

    # Отправляем сигнал остановки writer-у
    output_queue.put(None)
    writer_thread.join()

    cap.release()
    out.release()

    elapsed = time.time() - start_time
    print(f"Обработка завершена! Сохранено в {args.output}")
    print(f"Обработано кадров: {frame_idx}")
    print(
        f"Затраченное время: {elapsed:.2f} сек. FPS обработки: {frame_idx / elapsed:.2f}")


if __name__ == '__main__':
    main()
