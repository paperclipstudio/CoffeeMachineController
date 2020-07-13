/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

//#include "mbed.h"
//#include "platform/mbed_thread.h"
#include "stm32f429i_discovery_lcd.h"
#include "stm32f429i_discovery_ts.h"
#include "stm32f429i_discovery.h"

// Blinking rate in milliseconds
#define BLINKING_RATE_MS 200
enum MACHINE_STATE {
    STARTUP,
    LOW_POWER,
    HOT_WATER,
    STEAM,
    FAULT
};
MACHINE_STATE state = FAULT;

/// Strings
unsigned char startUpTitle[16] = "Starting Up";
unsigned char hotWaterTitle[16] = "Heating Water";
unsigned char steamingTitle[16] = "Steaming";
unsigned char lowPowerTitle[16] = "Low power mode";
unsigned char faultTitle[16] = "Fault found";

/// Button labels
unsigned char onButtonLabel[16] = "Turn On";
unsigned char offButtonLabel[16] = "Turn Off";
unsigned char steamButtonLabel[16] = "Steam Mode";
unsigned char hotWaterButtonLabel[16] = "Hot Water";

// Error Messages
unsigned char waterTooHotError[32] = "Water became to hot";
unsigned char invaildState[32] = "Invaild State";

//// Graphics
// Only true if the display has changed, and needs to be updated.
bool updateDisplay = true;
int backgroundColor = LCD_COLOR_WHITE;

// Pin Names
#define TEMP_PIN PC_4;
#define HEATER_PIN PG_13;
#define HEATER2_PIN PG_14;


    
// Initialise the digital pin LED1 as an output
DigitalOut heater1(HEATER_PIN);
DigitalOut heater2(HEATER2_PIN);
AnalogIn temp(TEMP_PIN);
    
Serial pc(USBTX, USBRX);
LCD_DISCO_F429ZI lcd;
    
// Touch Screen SetUp
TS_StateTypeDef touchState;

class Button{
    public:
        Button(int16_t x, int16_t y, int16_t newWidth, int16_t newHeight, 
        uint32_t newColor, unsigned char* buttonMessage, LCD_DISCO_F429ZI display){
            X = x;
            Y = y;
            width = newWidth;
            height = newHeight;
            color = newColor;
            screen = display;
            message = buttonMessage;
        }

        int16_t X;
        int16_t Y;
        int16_t width;
        int16_t height;
        uint32_t color;
        LCD_DISCO_F429ZI screen;
        unsigned char* message;
        
    
        bool inside(int16_t touchX, int16_t touchY) {
            return touchX > X 
            && touchX < X + width 
            && touchY > Y
            && touchY < Y + height;
        }

        bool inside(TS_StateTypeDef in) {
            return inside(touchState.X, touchState.Y);
        }

        void showButton() {
            uint16_t prevousColor = lcd.GetTextColor();
            screen.SetBackColor(color);
            screen.SetTextColor(LCD_COLOR_BLACK);
            screen.FillRect(X,Y,width,height);
            screen.SetTextColor(color);
            thread_sleep_for(500);
            screen.FillRect(X+3, Y+3, width-6, height-6);
            screen.SetTextColor(LCD_COLOR_BLACK);
            /*
            uint8_t *messagePointer = message;
            uint16_t messageLength = 0;
            while (*message++) messageLength++;
            
            if (screen.GetFont()->Width * messageLength > width){
                width = messageLength * screen.GetFont()->Width + 8;
                showButton();
                return;
            }
            */
            screen.DisplayStringAt(X + width / 2, (Y + height/2), message,CENTER_MODE);
            screen.SetTextColor(prevousColor);
            screen.SetBackColor(backgroundColor);
        }
};

template<typename T>
class shiftArray{
    private:
        int head;
        int tail;
        int size;
        int inc(int value) {
            return (value + 1) % size;
        }
        T array[100];

        
    public:
        shiftArray<T>(int16_t arraySize) {
            head = 0;
            tail = 0;
            size = 100;
            for(int i = 0; i < size; i++) {
                array[i] = 0;
            }
        }

        bool isEmpty() {
            return head == tail;
        }

        bool isFull() {
            return inc(head) == tail;
        }

        void add(T item) {
            if(this->isFull()) {
                tail = inc(tail);
            }
            head = inc(head);
            array[head] = item;
        }

        T get(int index) {
            int newIndex = head - index;
            while(newIndex < 0) newIndex = newIndex + size;
            return array[newIndex];
        }

        int getSize() {
            return size;
        }
};

void changeState(MACHINE_STATE newState) {
    updateDisplay = true;
    state = newState;
    lcd.Clear(backgroundColor);
    wait_ms(50);
}




int main()
{
    shiftArray<float> tempHistory = shiftArray<float>(100);
    float tempArray[] = { 1.0, 0.5, 0.0, 0.75, 0.6};
    unsigned char faultReason1[100] = "No Reason Found";
    unsigned char *faultReason = faultReason1;
    
    Button *onButton       = new Button(50, 100, 100, 50, LCD_COLOR_GREEN,     onButtonLabel, lcd);
    Button *offButton      = new Button(50,  70, 100, 50, LCD_COLOR_RED,       offButtonLabel, lcd);
    Button *steamButton    = new Button(50, 140, 150, 50, LCD_COLOR_LIGHTBLUE, steamButtonLabel, lcd);
    Button *hotWaterButton = new Button(50, 140, 150, 50, LCD_COLOR_LIGHTBLUE,      hotWaterButtonLabel, lcd);

    changeState(STARTUP);

    while (true) {
        switch(state){
            case STARTUP: {
                lcd.Clear(LCD_COLOR_WHITE);
                lcd.DisplayStringAtLine(2, startUpTitle);
                BSP_TS_Init(lcd.GetXSize(), lcd.GetYSize());
                thread_sleep_for(500);
                changeState(LOW_POWER);
                break;
            }

            case  LOW_POWER :{
                BSP_TS_GetState(&touchState);
                if(touchState.TouchDetected) {
                    if (touchState.X > onButton->X && touchState.X < (onButton->X + onButton->width) &&
                    touchState.Y > onButton->Y && touchState.Y < onButton->Y + onButton->width){
                        changeState(HOT_WATER);
                        break;
                    }
                }
                if (updateDisplay){
                    lcd.Clear(LCD_COLOR_WHITE);
                    onButton->showButton();
                    lcd.DisplayStringAtLine(2, lowPowerTitle);
                    updateDisplay = false;
                }
                thread_sleep_for(1000);
                break;
            }
            case HOT_WATER: {
                BSP_TS_GetState(&touchState);
                
                int16_t radius = 10 * temp;
                if (radius > 0) {
                        lcd.SetTextColor(LCD_COLOR_WHITE);
                        lcd.FillCircle(100, 200, (int16_t) 10);
                        lcd.SetTextColor(LCD_COLOR_BLACK);
                        lcd.FillCircle(100, 200, (int16_t) radius);
                }
                if (updateDisplay) {
                    lcd.Clear(LCD_COLOR_WHITE);
                    offButton->showButton();
                    steamButton->showButton();
                    lcd.ClearStringLine(2);
                    lcd.DisplayStringAtLine(2, hotWaterTitle);
                    updateDisplay = false;
                }
                if(touchState.TouchDetected) {
                    if(offButton->inside(touchState)){
                        heater1 = false;
                        heater2 = false;
                        changeState(LOW_POWER);
                        break;
                    }
                    if (steamButton->inside(touchState)) {
                        changeState(STEAM);
                        break;
                    }
                }
                if (temp < 0.5) {
                    heater1 = true;
                    if (temp < 0.3) {
                        heater2 = true;
                    } else {
                        heater2 = false;
                    }
                } else if (temp < 0.9) {
                    heater1 = false;
                    heater2 = false;
                } else {
                    changeState(FAULT);
                    faultReason = waterTooHotError;
                }
                break;
            }
            case STEAM: {
                if (updateDisplay) {
                    lcd.DisplayStringAtLine(2, steamingTitle);
                    offButton->showButton();
                    hotWaterButton->showButton();
                    lcd.DisplayStringAtLine(2, steamingTitle);
                    updateDisplay = false;
                }
                BSP_TS_GetState(&touchState);
                if(touchState.TouchDetected) {
                    if(offButton->inside(touchState)){
                        changeState(LOW_POWER);
                        break;
                    }
                    if (hotWaterButton->inside(touchState)) {
                        changeState(HOT_WATER);
                        break;
                    }
                }
                int16_t radius = 100 * temp;
                if (radius > 0) {
                    lcd.SetTextColor(LCD_COLOR_GRAY);
                    lcd.FillRect(200, 200, 25, (int16_t) 100);
                    lcd.SetTextColor(LCD_COLOR_BLACK);
                    lcd.FillRect(200, 200, 25, (int16_t) radius);
                }
                if (temp < 0.7) {
                    heater1 = true;
                    if (temp < 0.5) {
                        heater2 = true;
                    } else {
                        heater2 = false;
                    }
                } else if (temp < 0.9) {
                    heater1 = false;
                    heater2 = false;
                } else {
                    changeState(FAULT);
                    faultReason = waterTooHotError;
                }
                break;
            }
            case FAULT: {
                if (updateDisplay) {
                    lcd.DisplayStringAtLine(2, faultTitle);
                    lcd.DisplayStringAtLine(3, faultReason);
                    updateDisplay = false;
                }
                exit(0);
            }
            default: {
                changeState(FAULT);
                faultReason = invaildState;
            }
        }
        pc.printf("Temp:%0.2f Touch:%1d X:%3d Y:%3d \n", 
        temp.read(), touchState.TouchDetected, touchState.X, touchState.Y);
        tempHistory.add(temp.read());
        // Draw Graph;
        lcd.SetTextColor(LCD_COLOR_GREEN);
        lcd.FillRect(0, lcd.GetYSize()-50, 200, 50);
        lcd.SetTextColor(LCD_COLOR_BLACK);
        for (int i = 0; i < tempHistory.getSize(); i++) {   
            int x = i * 2;
            int width = 2;
            int height = tempHistory.get(i) *  50;
            int y = lcd.GetYSize() - height;
            //pc.printf("temp: %0.3f, x:%3d, y:%3d, width:%3d, height: %3d\n",tempHistory.get(i),x,y,width,height);
            lcd.DrawRect(x, y, width, height);
        }
        
        thread_sleep_for(BLINKING_RATE_MS);

    }



}


