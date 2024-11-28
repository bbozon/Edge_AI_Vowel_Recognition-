/*
  Vowel recoginizer
  by Bart Bozon
  https://www.youtube.com/@bartbozon
*/
#include <TinyMLShield.h>
// Arduino_TensorFlowLite - Version: 0.alpha.precompiled
#include <TensorFlowLite.h>

#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <tensorflow/lite/version.h>
#include <Arduino_APDS9960.h>
#include "model.h"
#include <PDM.h>
#include "arduinoFFT.h"
#include "names.h"  // This is used to read the CLASSES

// PDM buffer
short sampleBuffer[256];
volatile int samplesRead;

bool record = true;
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
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, samples, samplingFrequency);

// global variables used for TensorFlow Lite (Micro)
tflite::MicroErrorReporter tflErrorReporter;

// pull in all the TFLM ops, you can remove this line and
// only pull in the TFLM ops you need, if would like to reduce
// the compiled size of the sketch.
tflite::AllOpsResolver tflOpsResolver;

const tflite::Model* tflModel = nullptr;
tflite::MicroInterpreter* tflInterpreter = nullptr;
TfLiteTensor* tflInputTensor = nullptr;
TfLiteTensor* tflOutputTensor = nullptr;

// Create a static memory buffer for TFLM, the size may need to
// be adjusted based on the model you are using
constexpr int tensorArenaSize = 8 * 1024;
byte tensorArena[tensorArenaSize];

// array to map gesture index to a name, this is replaced by the names.h
/*
const char* CLASSES[] = {
  "e ",  // u8"\U0001F34E", // Apple
  "a  ",  // u8"\U0001F34C", // Banana
  "ee  ",  // u8"\U0001F34A"  // Orange
  "i  ",
  "o  ",
  " ",
  "u  "
};
*/

#define NUM_CLASSES (sizeof(CLASSES) / sizeof(CLASSES[0]))

void setup() {
  Serial.begin(9600);
  while (!Serial) {};

  // get the TFL representation of the model byte array
  tflModel = tflite::GetModel(model);
  if (tflModel->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model schema mismatch!");
    while (1)
      ;
  }

  // Initialize the TinyML Shield
  initializeShield();

  PDM.onReceive(onPDMdata);
  // Initialize PDM microphone in mono mode with 16 kHz sample rate
  if (!PDM.begin(1, 16000)) {
    Serial.println("Failed to start PDM");
    while (1)
      ;
  }

  Serial.println("Welcome to this vowel recognition application on the Nano 33 BLE Sense\n");
  Serial.println("It will try to recognize the following vowels:\n");
  for (int i = 0; i < NUM_CLASSES; i++) {
    Serial.print(CLASSES[i]);
  }

  // Create an interpreter to run the model
  tflInterpreter = new tflite::MicroInterpreter(tflModel, tflOpsResolver, tensorArena, tensorArenaSize, &tflErrorReporter);

  // Allocate memory for the model's input and output tensors
  tflInterpreter->AllocateTensors();

  // Get pointers for the model's input and output tensors
  tflInputTensor = tflInterpreter->input(0);
  tflOutputTensor = tflInterpreter->output(0);
}

void loop() {
  // see if the button is pressed and turn off or on recording accordingly
  bool clicked = readShieldButton();
  if (clicked) {
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

      //FFT = arduinoFFT(vReal, vImag, samples, samplingFrequency); /* Create FFT object */
      FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);            /* Weigh data */
      FFT.compute(FFT_FORWARD);                                   /* Compute FFT */
      FFT.complexToMagnitude();                                   /* Compute magnitudes */
      double x = FFT.majorPeak();
      //Serial.print(x, 6);

      // input sensor data to model
      for (int i = 0; i < samples; i++) {
        tflInputTensor->data.f[i] = (float)vReal[i];
      }
      // Run inferencing
      TfLiteStatus invokeStatus = tflInterpreter->Invoke();
      if (invokeStatus != kTfLiteOk) {
        Serial.println("Invoke failed!");
        while (1)
          ;
        return;
      }
      /*
      // Output results
      for (int i = 0; i < NUM_CLASSES; i++) {
        Serial.print(CLASSES[i]);
        Serial.print(" ");
        Serial.print(int(tflOutputTensor->data.f[i] * 100));
        Serial.print("%\n");
      }
      Serial.println();
      */
      for (int i = 0; i < NUM_CLASSES; i++) {
        if (int(tflOutputTensor->data.f[i] * 100) > 50) {
          Serial.println(CLASSES[i]);
        }
      }
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
