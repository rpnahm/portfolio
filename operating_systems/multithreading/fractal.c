/*
fractal.c - Sample Mandelbrot Fractal Display
Starting code for CSE 30341 Project 3.
*/

#include "gfx.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <complex.h>

/*
Compute the number of iterations at point x, y
in the complex space, up to a maximum of maxiter.
Return the number of iterations at that point.

This example computes the Mandelbrot fractal:
z = z^2 + alpha

Where z is initially zero, and alpha is the location x + iy
in the complex plane.  Note that we are using the "complex"
numeric type in C, which has the special functions cabs()
and cpow() to compute the absolute values and powers of
complex values.
*/

static int compute_point( double x, double y, int max )
{
	double complex z = 0;
	double complex alpha = x + I*y;

	int iter = 0;

	while( cabs(z)<4 && iter < max ) {
		z = cpow(z,2) + alpha;
		iter++;
	}

	return iter;
}

/*
Compute an entire image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax).
*/

void compute_image( double xmin, double xmax, double ymin, double ymax, int maxiter )
{
	int i,j;

	int width = gfx_xsize();
	int height = gfx_ysize();

	// For every pixel i,j, in the image...

	for(j=0;j<height;j++) {
		for(i=0;i<width;i++) {

			// Scale from pixels i,j to coordinates x,y
			double x = xmin + i*(xmax-xmin)/width;
			double y = ymin + j*(ymax-ymin)/height;

			// Compute the iterations at x,y
			int iter = compute_point(x,y,maxiter);

			// Convert a iteration number to an RGB color.
			// (Change this bit to get more interesting colors.)
			int gray = 255 * iter / maxiter;
			gfx_color(gray,gray,gray);

			// Plot the point on the screen.
			gfx_point(i,j);
		}
	}
}

void rescale(double *xmin, double *xmax, double *ymin, double *ymax) {
	int width = gfx_xsize();
	int height = gfx_ysize();
	int x_click = gfx_xpos();
	int y_click = gfx_ypos();

	// calculate range of mandlebrot calcs
	double x_range = *xmax - *xmin;
	double y_range = *ymax - *ymin;

	// compute the ratio across the screen of the click (0-1)
	double x_ratio = 1 - (double)(width - x_click) / width;
	double y_ratio = 1 - (double)(height - y_click) / height;

	// impose this mid point onto the mandelbrot range
	double x_mid = (x_ratio * x_range) + *xmin;
	double y_mid = (y_ratio * y_range) + *ymin;

	// calculate the new min and maxes
	*xmin = x_mid - (x_range / 2);
	*xmax = *xmin + x_range;
	*ymin = y_mid - (y_range / 2);
	*ymax = *ymin + y_range;

}

int main( int argc, char *argv[] )
{
	// The initial boundaries of the fractal image in x,y space.
	double zoom_factor = 1.25;
	double xmin=-1.5;
	double xmax= 0.5;
	double ymin=-1.0;
	double ymax= 1.0;

	// Maximum number of iterations to compute.
	// Higher values take longer but have more detail.
	int maxiter=25;

	// Open a new window.
	gfx_open(640,480,"Mandelbrot Fractal");

	// Show the configuration, just in case you want to recreate it.
	printf("coordinates: %lf %lf %lf %lf\n",xmin,xmax,ymin,ymax);
	// Fill it with a dark blue initially.
	gfx_clear_color(0,0,255);
	gfx_clear();
	// Display the fractal image
	compute_image(xmin,xmax,ymin,ymax,maxiter);

	while(1) {
		// Wait for a key or mouse click.
		int c = gfx_wait();

		// change the window
		/* 
		Up arrow = 		zoom in 
		Down Arrow = 	zoom out
		Click = 		Center on click location
		"m" key = 		more iterations
		"l" key = 		less iterations
		*/
	
		if (c == 131 || c == 133 || c == 1 || c == 108 || c == 109) {
			// zoom in 
			if (c == 131) {
				double xcenter = (xmin + xmax) / 2.0;
				double ycenter = (ymin + ymax) / 2.0;
				xmin = xcenter - ((xcenter - xmin) / zoom_factor);
				xmax = xcenter + ((xmax - xcenter) / zoom_factor);
				ymin = ycenter - ((ycenter - ymin) / zoom_factor);
				ymax = ycenter + ((ymax - ycenter) / zoom_factor);
				maxiter *= zoom_factor;
			}
			else if (c == 133) {
				double xcenter = (xmin + xmax) / 2.0;
				double ycenter = (ymin + ymax) / 2.0;
				xmin = xcenter - ((xcenter - xmin) * zoom_factor);
				xmax = xcenter + ((xmax - xcenter) * zoom_factor);
				ymin = ycenter - ((ycenter - ymin) * zoom_factor);
				ymax = ycenter + ((ymax - ycenter) * zoom_factor);
				maxiter /= zoom_factor;
			}
			else if (c == 1) {
				rescale(&xmin, &xmax, &ymin, &ymax);
			}
			else if (c == 108) {
				// reduce iterations 
				maxiter /= zoom_factor;
			}
			else if (c == 109) {
				// increase iterations
				maxiter *= zoom_factor;
			}
			
			printf("coordinates: %lf %lf %lf %lf, iterations: %d\n",xmin,xmax,ymin,ymax, maxiter);
				// Fill it with a dark blue initially.
			gfx_clear_color(0,0,255);
			gfx_clear();
			// Display the fractal image
			compute_image(xmin,xmax,ymin,ymax,maxiter);
		}
		// Quit if q is pressed.
		if(c=='q') exit(0);
	}

	return 0;
}
