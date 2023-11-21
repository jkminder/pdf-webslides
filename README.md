# PDF Web Slides Visualizer
This tool allows converting PDF presentation slides to a self-contained HTML5 file and is a modification of the original PDF Web Slides Tool. It is intended for the online visualisation of PDF slides for presentation viewers (instead of a presentator).



By pressing `g`+slidenumber you can directly go to a desired slide. By pressing `tab`, you can open the full slide overview. With `p` you can activate a pointer.

![PDF Web Slides in presentation mode](screenshot.png)
*Standart PDF visualization (left) with an overview of the next slides, and the full overview view (right)*

Differences to the [PDF Web Slides Tool](https://github.com/misc0110/pdf-webslides):
-   Shows a scrollable overview of the slides to the right of the current slide.
-   Has higher quality thumbnails
-   Removes the presentator mode and limits users to a single view.

# Usage

The first step is to convert a PDF to an HTML5 file. This is simply done by running

    pdf-webslides <pdf file>
    
The output is an `index.html` file and a corresponding `slides.js` in the current directory. Note that it is also possible to generate a standalone `index.html` using the `-s` option. If the HTML file is opened, it shows the slides in the same way as the original PDF. Slides can simply be navigated using left/right arrow keys, page-up/page-down keys, as well as by swiping over the slides. 


### Keyboard Shortcuts

| Key | Description |
|--|--|
| Right / Page Down / Swipe Right to Left |  Go to next slide                                  |
| Left / Page Up / Swipe Left to Right | Go to previous slide                            |
| Home | Go to first slide |
| End | Go to last slide |
| g   | Input slide number to go to |
| p   | Display/hide a pointer (i.e., a virtual laser pointer) |
| Tab   | Display the full slide overview. |

### Videos

PDF Web Slides has a rudimentary support for videos. 
Videos can be used in the same way as with pdfpc, e.g., using [pdfpc-commands.sty](https://github.com/dcherian/tools/blob/master/latex/pdfpc-commands.sty). 
Note that videos are not embedded into the HTML file, they have to be in the same folder. 

# Installation

### Linux

The tool depends on Poppler and Cairo for converting the PDF to SVGs. 
On Ubuntu, the dependencies can be installed through 

    apt install libcairo2-dev libpoppler-glib-dev
    
Then, the tool can be built by running

    make
    
This will generate a binary "pdf-webslides" that you can use to generate the html files. 

### Windows

On Windows, the tool can be built using MSYS2. 
First, install the required libraries and tools


    pacman -S gcc make mingw64/mingw-w64-x86_64-cairo mingw64/mingw-w64-x86_64-pkg-config mingw64/mingw-w64-x86_64-poppler 
   
Then, the tool can be built by running

    make -f Makefile.win
	
