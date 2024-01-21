/*
  Active Learning Labs
  Harvard University 
  tinyMLx - Built-in Microphone Test
*/

#include <PDM.h>
#include <TinyMLShield.h>
#include "arduinoFFT.h"

// PDM buffer
short sampleBuffer[256];
volatile int samplesRead;

bool record = false;
bool commandRecv = false;
const double signalFrequency = 1000;
const double samplingFrequency = 16000;
const uint8_t amplitude = 100;
const uint16_t samples = 256;
double vReal[samples];
double vImag[samples];
#define SCL_INDEX 0x00
#define SCL_TIME 0x01
#define SCL_FREQUENCY 0x02
#define SCL_PLOT 0x03
arduinoFFT FFT;

void setup() {
  Serial.begin(9600);
  while (!Serial)
    ;

  // Initialize the TinyML Shield
  initializeShield();

  PDM.onReceive(onPDMdata);
  // Initialize PDM microphone in mono mode with 16 kHz sample rate
  if (!PDM.begin(1, 16000)) {
    Serial.println("Failed to start PDM");
    while (1)
      ;
  }

  Serial.println("Welcome to the microphone test for the built-in microphone on the Nano 33 BLE Sense\n");
  Serial.println("Use the on-shield button or send the command 'click' to start and stop an audio recording");
  Serial.println("Open the Serial Plotter to view the corresponding waveform");
  double ratio = twoPi * signalFrequency / samplingFrequency;  // Fraction of a complete cycle stored at each sample (in radians)
  for (uint16_t i = 0; i < samples; i++) {
    vReal[i] = int8_t(amplitude * sin(i * ratio) / 2.0); /* Build data with positive and negative values*/
    //vReal[i] = uint8_t((amplitude * (sin(i * ratio) + 1.0)) / 2.0);/* Build data displaced on the Y axis to include only positive values*/
    vImag[i] = 0.0;  //Imaginary part must be zeroed in case of looping to avoid wrong calculations and overflows
  }
}

void loop() {
  // see if the button is pressed and turn off or on recording accordingly
  bool clicked = readShieldButton();
  if (clicked) {
    record = !record;
  }

  // see if a command was sent over the serial monitor and record it if so
  String command;
  while (Serial.available()) {
    char c = Serial.read();
    if ((c != '\n') && (c != '\r')) {
      command.concat(c);
    } else if (c == '\r') {
      commandRecv = true;
      command.toLowerCase();
    }
  }

  // parse the command if applicable
  if (commandRecv && command == "click") {
    commandRecv = false;
    record = !record;
  }

  // display the audio if applicable
  if (samplesRead) {
    // print samples to serial plotter
    if (record) {
      for (uint16_t i = 0; i < samples; i++) {
        vReal[i] = sampleBuffer[i];
        vImag[i] = 0.0;  //Imaginary part must be zeroed in case of looping to avoid wrong calculations and overflows
      }

      FFT = arduinoFFT(vReal, vImag, samples, samplingFrequency); /* Create FFT object */
      FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);            /* Weigh data */
      FFT.Compute(FFT_FORWARD);                                   /* Compute FFT */
      FFT.ComplexToMagnitude();                                   /* Compute magnitudes */
      //Serial.println("Computed magnitudes:");
      //PrintVector(vReal, (samples >> 1), SCL_FREQUENCY);
      double x = FFT.MajorPeak();
      //Serial.print(x, 6);
      //Serial.print(" ,  ");
      for (uint16_t i = 0; i < samples; i++) {
        Serial.print(vReal[i]);
        if (i<(samples-1))
          Serial.print(",");
      }
      Serial.println();


      //Serial.println(samplesRead);
      //for (int i = 0; i < samplesRead; i++) {
      //  Serial.println(sampleBuffer[i]);
      //}
    }
    // clear read count
    samplesRead = 0;
  }
}

void onPDMdata() {
  // query the number of bytes available
  int bytesAvailable = PDM.available();

  // read data into the sample buffer
  PDM.read(sampleBuffer, bytesAvailable);

  samplesRead = bytesAvailable / 2;
}

void PrintVector(double *vData, uint16_t bufferSize, uint8_t scaleType) {
  for (uint16_t i = 0; i < bufferSize; i++) {
    double abscissa;
    /* Print abscissa value */
    switch (scaleType) {
      case SCL_INDEX:
        abscissa = (i * 1.0);
        break;
      case SCL_TIME:
        abscissa = ((i * 1.0) / samplingFrequency);
        break;
      case SCL_FREQUENCY:
        abscissa = ((i * 1.0 * samplingFrequency) / samples);
        break;
    }
    Serial.print(abscissa, 6);
    if (scaleType == SCL_FREQUENCY)
      Serial.print("Hz");
    Serial.print(" ");
    Serial.println(vData[i], 4);
  }
  Serial.println();
}