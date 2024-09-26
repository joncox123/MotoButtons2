## Upload the Microcontroller Software
The easiest way to do this would be with a custom installer. However, that work is not yet done, so you will need to [download the Arduino IDE](https://www.arduino.cc/en/software), install the support package for the microcontroller, and then upload via a USB cable. The installation steps are [described here.](https://wiki.seeedstudio.com/XIAO_BLE/#getting-started)

First, open the Arduino IDE and then open Preferences in the File menu. Paste the URL (https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json) to the microcontroller board support package into the "Additional board managers URLs" text field. Click OK. 

<img src="AddBSP.PNG" alt="add Seeed BSP" width="600"/>

Second, install the Seeed nRF Boards package by clicking on the side button that is second from the top, to open the Board Manager. Type "nRF" into the search field at the top and install the "Seeed nRF52 Boards" package. 

<img src="InstallBSP.PNG" alt="install Seeed BSP" width="600"/>

Third, [download the source code file](../ArduinoCode/MotoButtonsLite.ino) and open in the Arduino IDE. Attach the microcontroller via a USB-C cable. Select the board that you just attached with the drop down box in the upper toolbar. It should say "Seeed XIAO nRF52840".

Finally, click the upload button!

<img src="upload.png" alt="install Seeed BSP" width="600"/>

## Updating the Software for Future Releases
To update the software again, you need to open the case and connect the USB-C port to your computer. Launch the Arduino IDE and open the new source code file. However, there is a bug in the microcontroller's DFU (Device Firmware Update) bootloader that sometimes prevents new software from being uploaded unless a special reset procedure is followed. 

If you try to upload new software and you either get an error stating the DFU upload failed, or it simply times out after about 60 seconds, you probably need to perform the special reset. There are two options to perform this reset. The first option is to hold down buttons A and B for 10 seconds, as if you were changing the mode. This should reset the controller into DFU mode again. The second option is to rapidly double tap the tiny reset button that is next to the USB-C connector. However, this is hard to do because it is so small, and it requires opening the case, which could be impossible if it is filled with epoxy and you can't get to the button. 

After resetting into DFU mode, keep in mind that the COM port (serial port) number of the microcontroller will have changed. So you will need to select it again in the drop down menu in the toolbar at the top of the Arduino IDE. Then you can proceed to upload the new code.
