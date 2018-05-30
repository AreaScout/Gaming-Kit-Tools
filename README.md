# Gaming-Kit-Tools

![Gaming-Kit-Tools](http://www.hardkernel.com/main/_Files/prdt/2018/201805/201805120009102637.jpg)

## Building
```
$ sudo apt-get install libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev
$ git clone https://github.com/AreaScout/Gaming-Kit-Tools.git
$ cd Gaming-Kit-Tools
$ make
$ sudo make install
```

## Usage

__gpio_button tool:__

```
Usage: ./gpio_button [-f] <cmd> [-r] <cmd> [-d]
        [-f] <cmd> command to execute when button is pressed
        [-r] <cmd> command to execute when button is released
        [-d] run as daemon

```

First run it as root with no command lime option, the gpio device sysfs entry has to be created 
After that you can add your script that has to be started when the right user button is pressed

__img2fb tool:__

```
Usage: ./img2fb [-i] <file> [-d]
        [-i] <file> valid image file types are JPG, PNG, TIF, WEBP
        [-d] disable aspect ratio
```

This tool displays an image on the LCD Gaming Kit, you don't need root rights for it

__show_info tool:__

```
Usage: ./show_info [-d] <num>
        Dim amount for OSD overlay (0 - 255, default 128)
```
		
Displays an info screen on the display, the current display contains is used as a background which you
dim with a user defined amount, if the tool exits it writes back the display conatins

__osd2fb tool:__

```
Usage: ./osd2fb [-f] <font> [-i] <msg> [-s] [-x] [-y] [-c] [-w] [-o]
        [-f] <file> your TTF font to load
        [-i] <msg> string to display
        [-s] outline the string
        [-x] <num> x offset for the string, usually 0-340
        [-y] <num> y offset for the strinf, usually 0-240
        [-c] center the string
        [-w] <num> weight/size of the font (default 15)
        [-o] <num> draw overlay with transparency 0-255
```

Writes user defined OSD to the LCD

example:

![Gaming-Kit-Tools](https://www.areascout.at/odroid_weather.png)
