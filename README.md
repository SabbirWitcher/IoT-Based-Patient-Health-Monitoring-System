# IoT-Based-Patient-Health-Monitoring-System
This is an IoT Based automated patient health monitoring system using ESP32 microcontroller and sensors. The sensors track vital signs and then generate alerts and notifications to send over the internet to the medical officers and family to alert about someone's health.


To run the main board Unit with pulse oxygen meter, body temperature sensor, gas sensor and ambient temperature with humidity sensor
  Open Main_Unit_with_Vital_Information.ino
  Connect the pins as mentioned in the comments of the code and upload

  
To run the extension unit ECG sensor to plot ecg 
  Open the ECG_with_TFT_Colab.ino and connect the circuit accordingly
  Open a colab file and upload the ECG_DATA_EVALUATION.ipynb change the link to the sheet to your Google Sheet
  Create a Google sheet and add the script sheet_script to the extension and deploy as Web App
  Then run the colab and ino file the data should update simulataneously in the LCD display and the colab output window



To run the Fall detection extension unit
  Open the gyro.ino file and connect the circuit accordingly
  Then create a Telegram Bot to send the notifications.
