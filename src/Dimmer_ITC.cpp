/*
  This file is part of the Dimmer_ITC library.
  Copyright (c) 2025 Ítalo Coelho. All rights reserved.
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
 *   _____  _                              _____ _______ _____ 
 *  |  __ \(_)                            |_   _|__   __/ ____|
 *  | |  | |_ _ __ ___  _ __ ___   ___ _ __ | |    | | | |     
 *  | |  | | | '_ ` _ \| '_ ` _ \ / _ \ '__|| |    | | | |     
 *  | |__| | | | | | | | | | | | |  __/ |  _| |_   | | | |____ 
 *  |_____/|_|_| |_| |_|_| |_| |_|\___|_| |_____|  |_|  \_____|
 *                                    ______                   
 *                                   |______|    
 *               
 *     Library for Phase Angle Control of AC Loads. The power output
 *  is linearly correlated to the selected level. A automatic method
 *  calibrating the zero-cross detection circuit is also included.
 * 
 *  Ítalo Coelho
 *          2025
 */

#include "Dimmer_ITC.h"

volatile uint8_t Dimmer_ITC::_level = 0;
volatile uint64_t Dimmer_ITC::_then = 0;
volatile float Dimmer_ITC::_frequency = 0;
volatile uint32_t Dimmer_ITC::_onTime = 0;
volatile uint32_t Dimmer_ITC::_offTime = 0;
volatile uint32_t Dimmer_ITC::_halfWave = 0;
volatile uint32_t Dimmer_ITC::_calibration = 192;

Dimmer_ITC* Dimmer_ITC::_instance = nullptr;


Dimmer_ITC::Dimmer_ITC(uint8_t zero_cross, uint8_t triac)
{
        _triac = triac;
        _zero_cross = zero_cross;
        
        _instance = this;
}


Dimmer_ITC::~Dimmer_ITC() {}


/*
        \brief Attach zero-cross interrupt and start control timer
*/
void Dimmer_ITC::begin()
{
        pinMode(_triac, OUTPUT);
        pinMode(_zero_cross, INPUT);

        digitalWrite(_triac, LOW);

        timer1_attachInterrupt(onTimerISR);
        timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
        attachInterrupt(digitalPinToInterrupt(_zero_cross), onZeroCrossISR, FALLING);
}


/*
    \brief Method to calibrate zero-cross detection circuit activation time
    \return Calibration success
*/
bool Dimmer_ITC::calibrate() 
{
        float freq = 0;
        uint64_t acc = 0, count = 0;
        uint64_t start, then;

        start = time();
        for(;;)
        {
                if(time() - start >= 1 * 1E+6)
                        return false;
                if(fabs(_frequency - 50) <= FREQ_VAR) freq = 50;
                if(fabs(_frequency - 60) <= FREQ_VAR) freq = 60;
                if(freq > 0) break;
                breathe();
        }
        _halfWave = (1E+6 / freq) / 2; //us

        then = time();
        start = time();
        bool previous = true;
        for(;;)
        {
                uint64_t now = time();
                if(now - start >= 1 * 1E+6)
                        break;
                bool state = !digitalRead(_zero_cross);
                if(state && !previous)
                        then = now;
                if(!state && previous)
                {
                        acc += now - then;
                        count++;
                }
                previous = state;
                breathe();
        }

        if(count == 0) return false;

        // Calculate delay between zero-cross and detection edge
        uint32_t calibration = (_halfWave - (acc / count)) / 2;

        if(calibration == 0) return false;
        if(calibration >= (1E+6 / 2) / 60) return false;

        _calibration = calibration;
        return true;
}


/*
    \brief Set dimmer power level
    \param Power level [0, 255]
*/
void Dimmer_ITC::setLevel(uint8_t level) 
{
        if(level == 0) 
        {
                timer1_disable();
                digitalWrite(_triac, LOW);
        } 
        else if(level == 255) 
        {
                timer1_disable();
                digitalWrite(_triac, HIGH);
        } 
        else 
        {
                _halfWave = (1E+6 / _frequency) / 2;
                _offTime = (calcAngle(level / 255.0) / PI) * _halfWave;
                _onTime = _halfWave - _offTime;
                if(_offTime < _calibration) 
                {
                        timer1_disable();
                        digitalWrite(_triac, HIGH);
                } 
                else if(_offTime + _calibration > _halfWave) 
                {
                        timer1_disable();
                        digitalWrite(_triac, LOW);
                } 
                else 
                {
                        timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
                }
        }
        _level = level;
}


/*
    \brief Set zero-cross detection calibration value
    \param Time between zero-cross and detection edge in µs
    \return Calibration value validity
*/
bool Dimmer_ITC::setCalibration(uint32_t calibration) 
{       
        if(calibration == 0) return false;
        if(calibration >= (1E+6 / 2) / 60) return false;
        _calibration = calibration;
        return true;
}


/*
        \brief Calculate activation angle corresponding to the power level
        \param Power level [0, 1.0]
*/
double Dimmer_ITC::calcAngle(double level)
{
        auto function = [](double level, double x)
        {
                return level - 1 + (x / PI) - (sin(2 * x) / TWO_PI);
        };

        //Bisection Method
        double a = 0.;
        double b = PI;
        double mid, f_mid;

        for (int i = 0; i < 1000; ++i)
        {
                mid = (a + b) / 2;
                f_mid = function(level, mid);

                if (fabs(f_mid) < 1E-16 || (b - a) < 1E-16)
                        return mid;

                if (function(level, a) * f_mid < 0)
                        b = mid;
                else
                        a = mid;
        }

        return mid;
}


/*
        \brief Get time in µs since boot
*/
uint64_t Dimmer_ITC::time()
{
#ifdef ESP32
        return esp_timer_get_time();
#elif ESP8266
        return micros64();
#else
        return micros();
#endif //MCU
}


/*
        \brief Allow for FreeRTOS / WDT to do its thing
*/
void Dimmer_ITC::breathe()
{
#ifdef ESP32
        vTaskDelay(pdMS_TO_TICKS(1));
#elif ESP8266
        yield();
#endif //MCU
}


/*
        \brief Zero-cross event ISR
*/
void IRAM_ATTR Dimmer_ITC::onZeroCrossISR() 
{
        uint64_t now = time();
        
        if(now - _then < DEBOUNCE) return;

        _frequency = 1E+6 / (now - _then);
        _then = now;

        timer1_write((_offTime - _calibration) * 5);
}


/*
        \brief Triac activation ISR
*/
void IRAM_ATTR Dimmer_ITC::onTimerISR() 
{
        digitalWrite(_instance->_triac, HIGH);
        delayMicroseconds(10);
        digitalWrite(_instance->_triac, LOW);
        timer1_write((_halfWave - 10) * 5);
}