// ThermistorProbe - Library for reading thermistor based temperature probes.
// Contributions and influence from @BDub and @avidan
// https://community.spark.io/t/thermistors-and-the-spark-core/1276

#include "ThermistorProbe.h"
#include "math.h"

#define DEBUG FALSE

// TermisterProbe constructor
// pur - Pullup resistor value in ohms
// adcRef - ADC reference value
ThermistorProbe::ThermistorProbe(double pur, int adc)
{
    _pur = pur;
    _adc = adc;
}

ThermistorProbe::~ThermistorProbe() {
    ;;;
}

// Returns the temperature in Kelvin based off of the Steinhart-Hart equation.
// http://en.wikipedia.org/wiki/Steinhart%E2%80%93Hart_equation
double ThermistorProbe::getTempK(int pin, enum ProbeType probeType, bool smooth)
{
    delay(1);

    if(DEBUG) {
        Serial.print("Pin: ");
        Serial.println(pin);
    }

    // Read in analog value
    int aval = analogRead(pin);

    // read in smoothly
    if(smooth==true) {
        int total = 0;

        for(int i=0; i<100; i++) {
            total += analogRead(pin);
            delay(1);
        }

        aval = total/100;
    } else {
        aval = analogRead(pin);
    }

    // define probe type
    _probeType = probeType;

    // select probe specific constants
    switch (_probeType) {
        case ET72:
            // ET-7, ET-72, ET-73, ET-72/73 probe constants
            A = 0.00023067431;
            B = 0.00023696594;
            C = 1.2636415e-7;
            break;
        case ET732:
            // ET-732 probe constants
            // https://github.com/CapnBry/HeaterMeter
            A = 0.00023067431;
            B = 0.00023696594;
            C = 1.2636415e-7;
            break;
        case IKEA: default:
            // IKEA probe constants
            double R1, R2, R3, T1, T2, T3;

            R1 = 62900;
            T1 = 18;

            R2 = 3285;
            T2 = 99.3;

            R3 = 160300;
            T3 = 0;

            T1 = T1 + 273.15;
            T2 = T2 + 273.15;
            T3 = T3 + 273.15;

            C =((1/T1-1/T2)-(log(R1)- log(R2))*(1/T1-1/T3)/(log(R1)-log(R3)))/((pow(log(R1),3)-pow(log(R2),3)) - (log(R1)-log(R2))*(pow(log(R1),3)-pow(log(R3),3))/(log(R1)-log(R3)));
            B =((1/T1-1/T2)-C*(pow(log(R1),3)-pow(log(R2),3)))/(log(R1)-log(R2));
            A = 1/T1-C*(log(R1))*(log(R1))*(log(R1))-B*log(R1);
            break;
    }

    if(DEBUG) {
        Serial.print("A: ");
        Serial.println(A,14);
        Serial.print("B: ");
        Serial.println(B,14);
        Serial.print("C: ");
        Serial.println(C,14);
        Serial.print("Analog value: ");
        Serial.println(aval);
        Serial.print("ADC ref: ");
        Serial.println(_adc);
    }

    // calculate resistance
    double R = (double)(1 / ((_adc / ((double) aval * 1)) - 1)) * (double)_pur;

    if(DEBUG) {
        Serial.print("R: ");
        Serial.println(R);
    }

    // calculate log10(R)
    R = log(R);

    if(DEBUG) {
        Serial.print("log(R): ");
        Serial.println(R);
    }

    // calculate temp
    double T = (1 / (A + B * R + C * R * R * R));

    if(DEBUG) {
        Serial.print("Temp: ");
        Serial.println(T);
        Serial.println();
    }

    // return temp
    return T;
}

// Returns temp in Celcius
double ThermistorProbe::getTempC(int pin, enum ProbeType probeType, bool smooth) {
    return getTempK(pin, probeType, smooth) - 273.15;
}

// Return temp in Fahrenheit
double ThermistorProbe::getTempF(int pin, enum ProbeType probeType, bool smooth) {
    return ((getTempC(pin, probeType, smooth) * 9.0) / 5.0 + 32.0);
}
