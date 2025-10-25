I first created a new ESP-IDF project by choosing the blink template.

I opened the project folder location in my system using Visual Studio Code.

In the main folder of the blink project, I replaced the existing main.c file with my own FreeRTOS implementation code that includes two tasks and a queue.

After replacing the code, I opened the ESP-IDF Command Prompt.

I navigated to the project folder using the command prompt path.

Inside the project directory, I noticed a CMake generator mismatch error while building for the first time, which was caused by different build systems (Ninja and MinGW).

To fix this issue, I removed the settings.json file from the .vscode folder and executed a full clean-up using the idf.py fullclean command.

After cleaning the build environment, I built the firmware again using idf.py build.

The build completed successfully without any errors or warnings.

I later flashed the compiled code to my ESP32 board using idf.py flash and monitored the output with idf.py monitor.

The firmware was tested successfully and validated that the FreeRTOS tasks and queue behaved as expected.

The results were verified in the serial monitor where task creation, priority changes, queue communication, and task deletion logs were visible.