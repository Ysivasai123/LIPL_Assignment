#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"

/* ============================================================================
   CONFIGURATION AND DEFINITIONS
   ============================================================================ */

#define TASK1_PRIORITY       2
#define TASK2_PRIORITY       3
#define QUEUE_LENGTH         5
#define TASK_SEND_PERIOD_MS  500
#define SEND_TIMEOUT_MS      100

/* Tag for logging */
static const char *TAG = "FREERTOS_DEMO";

/* ============================================================================
   DATA STRUCTURES
   ============================================================================ */

/**
 * @struct Data_t
 * @brief Structure containing data to be transmitted via queue
 */
typedef struct {
    uint8_t dataID;     /*!< Identifier for the data */
    int32_t DataValue;  /*!< Actual data value */
} Data_t;

/* ============================================================================
   GLOBAL VARIABLES
   ============================================================================ */

/* Global variables updated externally (simulator) */
volatile uint8_t G_DataID = 0;
volatile int32_t G_DataValue = 0;

/* Queue handle */
static QueueHandle_t Queue1 = NULL;

/* Task handles */
static TaskHandle_t TaskHandle_1 = NULL;
static TaskHandle_t TaskHandle_2 = NULL;

/* Store original priority of Task2 for restoration */
static UBaseType_t Task2_OriginalPriority = TASK2_PRIORITY;
static UBaseType_t Task2_CurrentPriority = TASK2_PRIORITY;
static uint8_t Task2_PriorityIncremented = 0;

/* ============================================================================
   FUNCTION PROTOTYPES
   ============================================================================ */

/**
 * @brief Example Task 1 - Producer task that sends data to queue
 * @param pV Void pointer to task parameters (not used)
 */
void ExampleTask1(void *pV);

/**
 * @brief Example Task 2 - Consumer task that processes queue data
 * @param pV Void pointer to task parameters (not used)
 */
void ExampleTask2(void *pV);

/**
 * @brief Simulate external update of global variables
 * @param pV Void pointer to task parameters (not used)
 */
void DataUpdateSimulator(void *pV);

/* ============================================================================
   TASK IMPLEMENTATIONS
   ============================================================================ */

/**
 * @brief ExampleTask1 - Producer Task
 * Sends data to Queue1 every 500ms with exact timing
 * 
 * Timing Strategy: Uses vTaskDelayUntil() for precise periodic execution
 * instead of vTaskDelay() to avoid timing drift accumulation.
 * 
 * @param pV Unused parameter
 */
void ExampleTask1(void *pV)
{
    Data_t data_to_send;
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(TASK_SEND_PERIOD_MS);
    
    /* Initialize the xLastWakeTime variable with current time */
    xLastWakeTime = xTaskGetTickCount();
    
    ESP_LOGI(TAG, "[Task1] Started - Producer Task");
    
    while (1) {
        /* Wait until it's time to send next data (500ms exactly) */
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        /* Capture global variables */
        data_to_send.dataID = (uint8_t)G_DataID;
        data_to_send.DataValue = (int32_t)G_DataValue;
        
        /* Send data to queue with timeout */
        if (xQueueSend(Queue1, &data_to_send, pdMS_TO_TICKS(SEND_TIMEOUT_MS)) == pdPASS) {
            ESP_LOGI(TAG, "[Task1] Sent - DataID: %u, DataValue: %ld", 
                     data_to_send.dataID, data_to_send.DataValue);
        } else {
            ESP_LOGW(TAG, "[Task1] Queue FULL - Failed to send data");
        }
    }
}

/**
 * @brief ExampleTask2 - Consumer Task
 * Receives data from Queue1 and processes based on dataID and DataValue
 * 
 * Processing Logic:
 * - dataID == 0: Delete self
 * - dataID == 1: Process DataValue
 *   - DataValue == 0: Increase priority by 2
 *   - DataValue == 1: Decrease priority back to original
 *   - DataValue == 2: Delete self
 * 
 * @param pV Unused parameter
 */
void ExampleTask2(void *pV)
{
    Data_t received_data;
    const TickType_t xReceiveTimeout = pdMS_TO_TICKS(1000); /* 1 second timeout */
    
    ESP_LOGI(TAG, "[Task2] Started - Consumer Task (Priority: %u)", 
             uxTaskPriorityGet(NULL));
    
    while (1) {
        /* Wait for data from queue (blocking with timeout) */
        if (xQueueReceive(Queue1, &received_data, xReceiveTimeout) == pdPASS) {
            /* Print received data */
            ESP_LOGI(TAG, "[Task2] Received - DataID: %u, DataValue: %ld", 
                     received_data.dataID, received_data.DataValue);
            
            /* === PROCESS DATAID === */
            if (received_data.dataID == 0) {
                /* dataID == 0: Delete ExampleTask2 */
                ESP_LOGI(TAG, "[Task2] DataID=0 detected - DELETING SELF");
                
                /* Reset priority tracking before deletion */
                Task2_PriorityIncremented = 0;
                
                /* Delete the current task */
                vTaskDelete(TaskHandle_2);
                /* Execution never reaches here */
            }
            else if (received_data.dataID == 1) {
                /* dataID == 1: Process DataValue */
                ESP_LOGI(TAG, "[Task2] DataID=1 - Processing DataValue: %ld", 
                         received_data.DataValue);
                
                /* === PROCESS DATAVALUE === */
                switch (received_data.DataValue) {
                    case 0:
                        /* DataValue == 0: Increase priority by 2 */
                        if (!Task2_PriorityIncremented) {
                            Task2_CurrentPriority = Task2_OriginalPriority + 2;
                            vTaskPrioritySet(TaskHandle_2, Task2_CurrentPriority);
                            Task2_PriorityIncremented = 1;
                            ESP_LOGI(TAG, "[Task2] Priority INCREASED to %u", Task2_CurrentPriority);
                        } else {
                            ESP_LOGW(TAG, "[Task2] Priority already increased");
                        }
                        break;
                    
                    case 1:
                        /* DataValue == 1: Decrease priority back to original */
                        if (Task2_PriorityIncremented) {
                            vTaskPrioritySet(TaskHandle_2, Task2_OriginalPriority);
                            Task2_CurrentPriority = Task2_OriginalPriority;
                            Task2_PriorityIncremented = 0;
                            ESP_LOGI(TAG, "[Task2] Priority DECREASED to %u", Task2_CurrentPriority);
                        } else {
                            ESP_LOGW(TAG, "[Task2] Priority was not increased - cannot decrease");
                        }
                        break;
                    
                    case 2:
                        /* DataValue == 2: Delete ExampleTask2 */
                        ESP_LOGI(TAG, "[Task2] DataValue=2 detected - DELETING SELF");
                        
                        /* Reset priority tracking before deletion */
                        Task2_PriorityIncremented = 0;
                        
                        /* Delete the current task */
                        vTaskDelete(TaskHandle_2);
                        /* Execution never reaches here */
                        break;
                    
                    default:
                        ESP_LOGD(TAG, "[Task2] DataValue=%ld - No action", received_data.DataValue);
                        break;
                }
            }
            else {
                ESP_LOGD(TAG, "[Task2] DataID=%u - Ignoring (not 0 or 1)", received_data.dataID);
            }
        }
        else {
            ESP_LOGD(TAG, "[Task2] Queue receive timeout");
        }
        
        /* Yield CPU to other tasks */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief Data Update Simulator - Updates global variables for testing
 * 
 * This task simulates external updates to G_DataID and G_DataValue
 * to demonstrate the consumer task's behavior.
 * 
 * Sequence:
 * 1. Send dataID=1, DataValue=0 (Test: Increase priority)
 * 2. Wait and send dataID=1, DataValue=1 (Test: Decrease priority)
 * 3. Wait and send dataID=1, DataValue=2 (Test: Delete task2)
 * 
 * @param pV Unused parameter
 */
void DataUpdateSimulator(void *pV)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(3000); /* 3 second intervals */
    uint32_t cycle = 0;
    
    ESP_LOGI(TAG, "[Simulator] Started - Will update globals every 3 seconds");
    
    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        switch (cycle % 4) {
            case 0:
                G_DataID = 1;
                G_DataValue = 0;
                ESP_LOGI(TAG, "[Simulator] Updated: DataID=1, DataValue=0 (Increase Priority)");
                break;
            
            case 1:
                G_DataID = 1;
                G_DataValue = 1;
                ESP_LOGI(TAG, "[Simulator] Updated: DataID=1, DataValue=1 (Decrease Priority)");
                break;
            
            case 2:
                G_DataID = 1;
                G_DataValue = 2;
                ESP_LOGI(TAG, "[Simulator] Updated: DataID=1, DataValue=2 (Delete Task2)");
                break;
            
            case 3:
                G_DataID = 1;
                G_DataValue = 0;
                ESP_LOGI(TAG, "[Simulator] Updated: DataID=1, DataValue=0 (Recreate scenario)");
                break;
        }
        
        cycle++;
    }
}

/* ============================================================================
   MAIN INITIALIZATION FUNCTION
   ============================================================================ */

/**
 * @brief Application entry point
 * Initializes FreeRTOS objects and creates tasks
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=== FreeRTOS Multi-Task Queue Demonstration ===");
    ESP_LOGI(TAG, "Creating Queue1 with size %d", QUEUE_LENGTH);
    
    /* Create Queue1 with specified size and data type */
    Queue1 = xQueueCreate(QUEUE_LENGTH, sizeof(Data_t));
    
    if (Queue1 == NULL) {
        ESP_LOGE(TAG, "Failed to create Queue1");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Queue1 created successfully");
    
    /* Create ExampleTask1 (Producer) */
    BaseType_t xReturned1 = xTaskCreate(
        ExampleTask1,          /* Task function */
        "ExampleTask1",        /* Task name */
        2048,                  /* Stack depth (words) */
        NULL,                  /* Task parameters */
        TASK1_PRIORITY,        /* Priority */
        &TaskHandle_1          /* Task handle */
    );
    
    if (xReturned1 == pdPASS) {
        ESP_LOGI(TAG, "ExampleTask1 created successfully (Priority: %d)", TASK1_PRIORITY);
    } else {
        ESP_LOGE(TAG, "Failed to create ExampleTask1");
    }
    
    /* Create ExampleTask2 (Consumer) */
    BaseType_t xReturned2 = xTaskCreate(
        ExampleTask2,          /* Task function */
        "ExampleTask2",        /* Task name */
        2048,                  /* Stack depth (words) */
        NULL,                  /* Task parameters */
        TASK2_PRIORITY,        /* Priority */
        &TaskHandle_2          /* Task handle */
    );
    
    if (xReturned2 == pdPASS) {
        ESP_LOGI(TAG, "ExampleTask2 created successfully (Priority: %d)", TASK2_PRIORITY);
    } else {
        ESP_LOGE(TAG, "Failed to create ExampleTask2");
    }
    
    /* Create Data Update Simulator Task */
    BaseType_t xReturned3 = xTaskCreate(
        DataUpdateSimulator,   /* Task function */
        "DataSimulator",       /* Task name */
        2048,                  /* Stack depth (words) */
        NULL,                  /* Task parameters */
        1,                     /* Priority (lowest) */
        NULL                   /* Task handle (not needed) */
    );
    
    if (xReturned3 == pdPASS) {
        ESP_LOGI(TAG, "DataUpdateSimulator created successfully");
    } else {
        ESP_LOGE(TAG, "Failed to create DataUpdateSimulator");
    }
    
    ESP_LOGI(TAG, "=== System Initialization Complete ===");
    
    /* FreeRTOS scheduler takes over from here */
}
