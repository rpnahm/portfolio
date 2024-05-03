# Thread Management Project
This project uses X11 to display the Mandelbrot fractal on your screen.
The final executable, fractal task breaks the work up into jobs of 20x20 pixels, and then has a specified number of threads work on each block for maximum performance. 
- The purpose of this project was to practice using mutex and conditional variables to handle multithreaded processing of a single job.

## Input
    Make sure that the graphics window is in focus in order for it to capture any keyboard input. 
    - Click anywhere on the screen to center the graphic there
    - use up arrow to zoom in and down arrow to zoom out
    - use "m" to increase the number of iterations, and "l" to decrease the number. 
    - use "q" to quit.

    Note the keyboard instructions are cached via stdin, so all keypresses while focused on the window are recorded and will execute in the order they were recorded even if the key was pressed during the rendering process for a previous image. 