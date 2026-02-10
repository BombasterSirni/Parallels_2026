# **Task №1**: *Заполнение массива значениями синуса*
### **Сборка задачи с выбором float/double:**
    1. Сборка задачи с массивом типа float (default):
    ```bash
    cd Task1
    cmake -B build
    cd build
    make
    ./sin_sum
    ```bash

    2. Сборка задачи с массивом типа double:
    ```bash
    cd Task1
    cmake -B build -DUSE_DOUBLE=ON
    cd build
    make
    ./sin_sum
    ```bash

### **Значения суммы массива**:
    1. тип float: -0.0040017021819949150085449
    2. тип double: 0.0000000000489581965450312
    