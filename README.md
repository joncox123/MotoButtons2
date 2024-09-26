# MotoButtons 2

### *MotoButtons 2 is a work in progress. The date of completion will be September 29, 2024. I had the case printed and ordered the remaining parts. I got a good deal on an industrial 4-way joystick, so I'll be writing the code for that option first.*

Welcome to MotoButtons 2, the low-cost, waterproof, DIY Bluetooth motorcycle controller. Version 2 features many upgrades over [the original](https://github.com/joncox123/MotoButtons):
- 3D printed case with handlebar mount
- Separate RGB LED for indicating modes and status
- Commercial, IP68 5-way (or 4-way) castle joystick
- Three pushbuttons

The target price to order all parts, including the case, is approximately $80 to $100 excluding shipping costs. The main variable is the cost and quality of the 3D printed case. However, the most expensive part is the IP68 rated 5-way joystick. The cheapest available is from Ruffy Controls, although industry prices for similar parts vary from $45 to beyond $300. Lastly, the least expensive service for 3D printing is [Craftcloud](https://craftcloud3d.com/).

## Assembly Steps

[1] Order your 3D printed case by uploading the three [CAD STEP files](./Case/Parts) to [Craftcloud](https://craftcloud3d.com/). Use the Nylon SLA material, as the cheaper PLA filament will not be of satisfactory quality. You do not need to pay for any special finishings or other options beyond Nylon SLA.

[2] Order the parts from Digikey by [clicking this link](https://www.digikey.com/short/hzjjppm1), as listed in the [Bill of Materials](./Parts/BOM.csv). Note, due to shipping costs, it is cheaper to order several HS joysticks from Ruffy Controls at once. I recommend ordering one for each bike, or organize a group buy on a forum.

[3] Read the [wiring diagram](./ConstructionGuide/README.md) to assemble your MotoButtons 2 after obtaining the parts and the case.

[4] Upload the software to the microcontroller using a USB-C cable and the Arduino program. See [this guide](./Programming/README.md) for detailed instructions.

<img src="Case/MB2_Case.PNG" alt="3D Printed Case Model" width="400"/>
