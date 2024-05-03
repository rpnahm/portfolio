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
#include <unistd.h>
#include <pthread.h>


// struct to contain the mandelbrot info
typedef struct{
	// for mandelbrot
	double xmin;
	double xmax;
	double ymin;
	double ymax;
	int maxiter;
	int width;
	int height;	
}Mand_info; 

// info for individual blocks
typedef struct {
	int xmin_g;
	int xmax_g;
	int ymin_g;
	int ymax_g;

	// stores the info that doesn't change for individual blocks
	Mand_info *mand; 
}Block;

// mutex for gfx 
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

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

void * safe_compute_image(void * arg) {
	Block * info = arg;

	for(int j=info->ymin_g;j<=info->ymax_g;j++) {
		for(int i=info->xmin_g;i<=info->xmax_g;i++) {

			// Scale from pixels i,j to coordinates x,y
			double x = info->mand->xmin + i * (info->mand->xmax - info->mand->xmin) / info->mand->width;
			double y = info->mand->ymin + j * (info->mand->ymax - info->mand->ymin) / info->mand->height;

			// Compute the iterations at x,y
			int iter = compute_point(x,y,info->mand->maxiter);

			// Convert a iteration number to an RGB color.
			int gray = 255 * iter / info->mand->maxiter;
			pthread_mutex_lock(&mutex);
			gfx_color(gray,gray,gray);

			// Plot the point on the screen.
			gfx_point(i,j);
			pthread_mutex_unlock(&mutex);
		}
	}
	return NULL;
}

/*
Compute an entire image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax).
*/

void compute_image( double xmin, double xmax, double ymin, double ymax, int maxiter, int threads)
{
	Mand_info * mand = malloc(sizeof(Mand_info));
	Block * info_arr = malloc(sizeof(Block) * threads);
	pthread_t * tid_arr = malloc(sizeof(pthread_t) * threads);
	
	int width = gfx_xsize();
	int height = gfx_ysize();
	
	// initialize the static struct
	mand->width = width;
	mand->height = height;
	mand->maxiter = maxiter;
	mand->xmin = xmin;
	mand->xmax = xmax;
	mand->ymin = ymin;
	mand->ymax = ymax;

	// intialize and call each thread
	int block_size = height / threads;
	int i;
	for (i = 0; i < threads - 1; i++) {
		info_arr[i].mand = mand;
		info_arr[i].xmin_g = 0;
		info_arr[i].xmax_g = width;
		info_arr[i].ymin_g = i * block_size;
		info_arr[i].ymax_g = (i + 1) * block_size - 1;
		pthread_create(&tid_arr[i], 0, safe_compute_image, &info_arr[i]);
	}
	// handle the last row (remainders)
	info_arr[i].mand = mand;
	info_arr[i].xmin_g = 0;
	info_arr[i].xmax_g = width;
	info_arr[i].ymin_g = i * block_size;
	info_arr[i].ymax_g = height;
	pthread_create(&tid_arr[i], 0, safe_compute_image, &info_arr[i]);

	// wait for all threads
	void * result;
	for (int i = 0; i < threads; i++) {
		pthread_join(tid_arr[i], &result);
	}
	free(info_arr);
	free(mand);
	free(tid_arr);
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
	int threads = 1; 

	// Maximum number of iterations to compute.
	// Higher values take longer but have more detail.
	int maxiter=500;

	// Open a new window.
	gfx_open(640,480,"Mandelbrot Fractal");

	// Show the configuration, just in case you want to recreate it.
	printf("coordinates: %lf %lf %lf %lf\n",xmin,xmax,ymin,ymax);
	// Fill it with a dark blue initially.
	gfx_clear_color(0,0,255);
	gfx_clear();
	// Display the fractal image
	compute_image(xmin,xmax,ymin,ymax,maxiter, 1);

	while(1) {
		// Wait for a key or mouse click.
		int c = gfx_wait();
		// Turn c as a char into an integer 
		int new_thread = atoi((char*) &c);
		int change = 0;

		if (new_thread >=1 && new_thread <= 8) {
			change = 1;
			threads = new_thread;
		}
		// change the window
		/* 
		Up arrow = 		zoom in 
		Down Arrow = 	zoom out
		Click = 		Center on click location
		"m" key = 		more iterations
		"l" key = 		less iterations
		*/
	
		if (c == 131 || c == 133 || c == 1 || c == 108 || c == 109 || change) {
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
			
			printf("coordinates: %lf %lf %lf %lf, iterations: %d, theads: %d\n",xmin,xmax,ymin,ymax, maxiter, threads);
				// Fill it with a dark blue initially.
			gfx_clear_color(0,0,255);
			gfx_clear();
			// Display the fractal image
			compute_image(xmin,xmax,ymin,ymax,maxiter, threads);
		}
		// Quit if q is pressed.
		if(c=='q') exit(0);
	}

	return 0;
}
