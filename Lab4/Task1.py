import cv2
import threading
import queue
import time
import argparse
import os
import logging


log_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'log')
os.makedirs(log_dir, exist_ok=True)
logging.basicConfig(
    filename=os.path.join(log_dir, 'Task1.log'),
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)


class Sensor:
    def get(self):
        raise NotImplementedError("Subclasses must implement method get()")


class SensorCam(Sensor):
    def __init__(self, camera_name, resolution):
        self.camera_name = camera_name
        self.resolution = resolution

        try:
            self.cam_id = int(self.camera_name)
        except ValueError:
            self.cam_id = self.camera_name

        self.cap = cv2.VideoCapture(self.cam_id)
        if not self.cap.isOpened():
            logging.error(f"Не удалось открыть камеру: {self.camera_name}. "
                          f"Возможно, камеры с таким именем нет в системе.")
            raise RuntimeError(
                f"Не удалось открыть камеру: {self.camera_name}")

        if self.resolution:
            try:
                width, height = map(int, self.resolution.lower().split('x'))
                self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
                self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
            except ValueError:
                logging.error(
                    f"Некорректный формат разрешения: {self.resolution}. Ожидается 'WIDTHxHEIGHT'")
                raise ValueError(
                    f"Некорректный формат разрешения: {self.resolution}")

    def get(self):
        if not self.cap.isOpened():
            logging.error(f"Камера {self.camera_name} внезапно отключилась.")
            return None

        ret, frame = self.cap.read()
        if not ret:
            logging.error(f"Ошибка чтения кадра с камеры: {self.camera_name}.")
            return None
        return frame

    def __del__(self):
        if hasattr(self, 'cap') and self.cap.isOpened():
            self.cap.release()


class SensorX(Sensor):
    def __init__(self, delay: float):
        self._delay = delay
        self._data = 0

    def get(self):
        time.sleep(self._delay)
        self._data += 1
        return self._data


class WindowImage:
    def __init__(self, freq):
        self.freq = freq
        self.window_name = "Sensors Data"
        self.delay_ms = max(1, int(1000.0 / self.freq))

        cv2.namedWindow(self.window_name, cv2.WINDOW_AUTOSIZE)

    def show(self, img):
        if img is not None:
            cv2.imshow(self.window_name, img)
            key = cv2.waitKey(self.delay_ms) & 0xFF
            return key == ord('q')
        return False

    def __del__(self):
        cv2.destroyAllWindows()


def cam_worker(sensor_obj, out_queue, stop_event):
    while not stop_event.is_set():
        data = sensor_obj.get()
        if data is None:
            stop_event.set()
            break

        # Очищаем очередь, чтобы хранить только самый последний кадр
        while not out_queue.empty():
            try:
                out_queue.get_nowait()
            except queue.Empty:
                break
        out_queue.put(data)


def sensor_worker(sensor_obj, out_queue, stop_event):
    while not stop_event.is_set():
        data = sensor_obj.get()

        # Очищаем очередь, чтобы хранить только последнее значение
        while not out_queue.empty():
            try:
                out_queue.get_nowait()
            except queue.Empty:
                break
        out_queue.put(data)


def main():
    parser = argparse.ArgumentParser(
        description="Task 1: Sensor reading and displaying")
    parser.add_argument('--camera', type=str, default='0',
                        help='Имя камеры в системе (например, 0 или /dev/video0)')
    parser.add_argument('--resolution', type=str, default='640x480',
                        help='Желаемое разрешение камеры (например, 1280x720)')
    parser.add_argument('--freq', type=float, default=30.0,
                        help='Частота отображения картинки (в Hz)')

    args = parser.parse_args()

    stop_event = threading.Event()

    # Очереди для взаимодействия между потоками
    cam_queue = queue.Queue(maxsize=1)
    s100_queue = queue.Queue(maxsize=1)
    s10_queue = queue.Queue(maxsize=1)
    s1_queue = queue.Queue(maxsize=1)

    threads = []

    try:
        cam_sensor = SensorCam(args.camera, args.resolution)

        s100_sensor = SensorX(0.01)
        s10_sensor = SensorX(0.1)
        s1_sensor = SensorX(1.0)

        window = WindowImage(args.freq)

        # Запуск потоков
        t_cam = threading.Thread(target=cam_worker, args=(
            cam_sensor, cam_queue, stop_event))
        t_100 = threading.Thread(target=sensor_worker, args=(
            s100_sensor, s100_queue, stop_event))
        t_10 = threading.Thread(target=sensor_worker, args=(
            s10_sensor, s10_queue, stop_event))
        t_1 = threading.Thread(target=sensor_worker, args=(
            s1_sensor, s1_queue, stop_event))

        threads.extend([t_cam, t_100, t_10, t_1])
        for t in threads:
            t.daemon = True  # Позволяет завершить потоки при выходе
            t.start()

        last_s100_data = "SensorX(100Hz): N/A"
        last_s10_data = "SensorX(10Hz): N/A"
        last_s1_data = "SensorX(1Hz): N/A"

        while not stop_event.is_set():
            # Получение самого нового кадра с минимальной задержкой
            try:
                frame = cam_queue.get(timeout=1.0)
            except queue.Empty:
                if not stop_event.is_set():
                    logging.error(
                        "Таймаут ожидания кадра с камеры. Камера могла отключиться.")
                    break
                continue

            # Неблокирующее получение самых свежих данных с остальных датчиков
            try:
                last_s100_data = f"SensorX(100Hz): {s100_queue.get_nowait()}"
            except queue.Empty:
                pass

            try:
                last_s10_data = f"SensorX(10Hz): {s10_queue.get_nowait()}"
            except queue.Empty:
                pass

            try:
                last_s1_data = f"SensorX(1Hz): {s1_queue.get_nowait()}"
            except queue.Empty:
                pass

            # Наложение текста
            cv2.putText(frame, last_s100_data, (10, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
            cv2.putText(frame, last_s10_data, (10, 60),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
            cv2.putText(frame, last_s1_data, (10, 90),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

            # Если 'q' нажата - выходим
            if window.show(frame):
                stop_event.set()
                break

    except Exception as e:
        logging.error(f"Непредвиденная ошибка: {e}")
        stop_event.set()
    finally:
        stop_event.set()


if __name__ == '__main__':
    main()
