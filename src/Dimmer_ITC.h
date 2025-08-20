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
  

#ifndef DIMMER_ITC_H
#define DIMMER_ITC_H

#include <Arduino.h>

#define FREQ_VAR 0.4 //Hz
#define DEBOUNCE ((1E+6 / 50) / 2) //µs

class Dimmer_ITC
{
    private:
      static Dimmer_ITC* _instance;

      uint8_t _triac;
      uint8_t _zero_cross;

      static volatile uint8_t _level;
      static volatile float _frequency;

      static volatile uint64_t _then;

      static volatile uint32_t _onTime;
      static volatile uint32_t _offTime;
      static volatile uint32_t _halfWave;
      static volatile uint32_t _calibration;

      static void breathe();
      static uint64_t time();

      static double calcAngle(double level);

      static void IRAM_ATTR onTimerISR();
      static void IRAM_ATTR onZeroCrossISR();

    public:
        Dimmer_ITC(uint8_t zero_cross, uint8_t triac);
        ~Dimmer_ITC();

        void begin();
        bool calibrate();

        void setLevel(uint8_t level);
        bool setCalibration(uint32_t calibration);

        uint8_t getLevel() const { return _level; }
        float getFrequency() const { return _frequency; }
        uint32_t getCalibration() const { return _calibration;}
        
};

#endif //DIMMER_ITC_H