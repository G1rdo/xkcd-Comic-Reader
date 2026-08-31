> "If you have an idea and publish it on the internet, that counts as a ghost of done." -Cult of Done
>
> This project is not completed, and for the foreseeable future not going to be completed by me.
> This was one of my first times writing code for a ESP32, and the code likely does not reflect best practices and probably should be redone from scratch.
> From what I can tell, the hardware should work, but has not been tested physically.

# xkcd Comic Reader

<img width="677" height="1036" alt="image" src="https://github.com/user-attachments/assets/f080811e-01f6-4fe8-b37c-cd80bf88cfed" />

Ever wanted a E-ink display to get your tri-weekly xkcd comics? This project is for you! This fully open source e-ink display mount/case uses an onboard ESP32-S3 to download and display xkcd comics as they are produced with minimal power use. It also uses a non-backlit E-ink display so that it won't keep you up at night with light if you want to keep it in your bedroom!

This comic reader was designed in Fusion360, using the following parts:
* [SUNMAXIC 3.7" E-Ink 240*416 px Display](https://www.aliexpress.com/i/3256808194165882.html)
* [XIAO ePaper Display Board(ESP32-S3) - EE04](https://www.aliexpress.us/item/3256809861809448.html)
* [FFC Male to Female Extender](https://www.adafruit.com/product/4524)
* [FFC Extension Cable](https://www.adafruit.com/product/4230)

Further improvements include adding a battery with a JST 2.0mm connector, as the board supports one. 

<img width="1280" height="1280" alt="Final Case v2 Front Non-Transparent" src="https://github.com/user-attachments/assets/173264b8-28bb-475a-94e3-edef077e2110" />

## Instructions
First, connect the E-Ink display via the FFC Extension cable to the PCB. Then, download this github repository and edit ./secret.h.example to include your WiFi password and name. Flash the firmware to the PCB (compile the xkcd-driver.ino file and send it over USB), and the screen should light up with the newest XKCD comic! It will automatically update every few days.

## License

This work is licensed under a [Creative Commons Attribution-NonCommercial 3.0 License](LICENSE) to reflect that of the [xkcd font](https://github.com/ipython/xkcd-font/) used in the Hack Club Fallout Zine.
