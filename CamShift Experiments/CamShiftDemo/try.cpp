/*
#ifdef _CH_
#pragma package <opencv>
#endif

#ifndef _EiC
#include <opencv\cv.h>
#include <opencv\highgui.h>
#include <stdio.h>
#include <ctype.h>
#endif

IplImage *image = 0, *hsv = 0, *hue = 0;					//IplImage is a structure from Intel Image Processing Library
IplImage *mask = 0, *backproject = 0, *histimg = 0;			//Which contains the image data like ROI, scale, height, width etc.	
CvHistogram *hist = 0;

using namespace std;

int cntVector[]={0,0,0,0,0,0,0,0};

//FLAGS
int backproject_mode = 0;
int select_object = 0;
int track_object = 0;
int show_hist = 1;

//PARAMETERS FOR SEARCH WINDOW
CvPoint origin;
CvRect selection;
CvRect track_window;
CvBox2D track_box;
CvConnectedComp track_comp;


int hdims = 16;							//USED TO PROVIDE THE BINS OF THE HISTOGRAM, THE MORE NUMBER, MORE BINS
float hranges_arr[] = {0,180};
float* hranges = hranges_arr;
float tempx=0, tempy=0;
int vmin = 10, vmax = 256, smin = 60;


//FUNCTION FOR THE EVENT OF MOUSE CLICK/DRAG
void on_mouse( int event, int x, int y, int flags, void* param )
{
    if( !image )
        return;

    if( image->origin )
        y = image->height - y;

    if( select_object )
    {
        selection.x = MIN(x,origin.x);
        selection.y = MIN(y,origin.y);
        selection.width = selection.x + CV_IABS(x - origin.x);
        selection.height = selection.y + CV_IABS(y - origin.y);
      
      
		selection.x = MAX( selection.x, 0 );
        selection.y = MAX( selection.y, 0 );
        selection.width = MIN( selection.width, image->width );
        selection.height = MIN( selection.height, image->height );
        selection.width -= selection.x;
        selection.height -= selection.y;
    }

	//CASE WHERE WE SELECT THE INITIAL ROI
    switch( event )
    {
    case CV_EVENT_LBUTTONDOWN:
        origin = cvPoint(x,y);
        selection = cvRect(x,y,0,0);
        select_object = 1;
        break;
    case CV_EVENT_LBUTTONUP:
        select_object = 0;
        if( selection.width > 0 && selection.height > 0 )
            track_object = -1;
        break;
    }
}

//FUNCTION TO CONVERT HSV VALUES TO RGB
CvScalar hsv2rgb( float hue )
{
    int rgb[3], p, sector;
    static const int sector_data[][3]=
        {{0,2,1}, {1,2,0}, {1,0,2}, {2,0,1}, {2,1,0}, {0,1,2}};
    hue *= 0.033333333333333333333333333333333f;
    sector = cvFloor(hue);
    p = cvRound(255*(hue - sector));
    p ^= sector & 1 ? 255 : 0;

    rgb[sector_data[sector][0]] = 255;
    rgb[sector_data[sector][1]] = 0;
    rgb[sector_data[sector][2]] = p;

    return cvScalar(rgb[2], rgb[1], rgb[0],0);
}

int main( int argc, char** argv )
{
	try
	{
		CvCapture* capture = 0;				//CvCapture is the standard structure used as a parameter for all video operations
    
		if( argc == 1 || (argc == 2 && strlen(argv[1]) == 1 && isdigit(argv[1][0])))
		{
			capture = cvCaptureFromCAM(argc == 2 ? argv[1][0] - '0' : 0);		//If source specified, use it. Else use default(0)
			IplImage* frame = cvQueryFrame(capture);							//Create image frames from capture
		}
		else if( argc == 2 )
			capture = cvCaptureFromAVI( argv[1] );								//Capture from AVI (not a case)

		if( !capture )															//Capture from camera failed
		{
			fprintf(stderr,"Could not initialize capturing...\n");
			return -1;
		}

		printf( "Hot keys: \n"
			"\tESC - quit the program\n"
			"\tc - stop the tracking\n"
			"\tb - switch to/from backprojection view\n"
			"\th - show/hide object histogram\n"
			"To initialize tracking, select the object with mouse\n" );

		cvNamedWindow( "Histogram", 1 );					//Create a new window for showing histogram
		cvNamedWindow( "CamShiftDemo", 1 );								//Main window
		cvSetMouseCallback( "CamShiftDemo", on_mouse, 0 );				//Set mouse handler for main window
		cvCreateTrackbar( "Vmin", "CamShiftDemo", &vmin, 256, 0 );
		cvCreateTrackbar( "Vmax", "CamShiftDemo", &vmax, 256, 0 );
		cvCreateTrackbar( "Smin", "CamShiftDemo", &smin, 256, 0 );


		/* Begin the loop for real-time capture
		* The loop executes until the ESC key is pressed, which indicates the termination of the program
		*
		for(;;)
		{
			IplImage* frame = 0;
			int i, bin_w, c;
			IplImage *hsv2;
			frame = cvQueryFrame( capture );					//cvQueryFrame captures the frame from the camera input
			if( !frame )
				break;

			if( !image )
			{
				image = cvCreateImage( cvGetSize(frame), 8, 3 );			//create the image from the current frame
				image->origin = frame->origin;
				hsv = cvCreateImage( cvGetSize(frame), 8, 3 );				//create the hsv, hue, mask and backproject variable,
																			//will be null now since the image is not selected
				hue = cvCreateImage( cvGetSize(frame), 8, 1 );
				mask = cvCreateImage( cvGetSize(frame), 8, 1 );
				backproject = cvCreateImage( cvGetSize(frame), 8, 1 );
				hist = cvCreateHist( 1, &hdims, CV_HIST_ARRAY, &hranges, 1 );				//range 0-180
				histimg = cvCreateImage( cvSize(320,200), 8, 3 );			//create the histogram image, size(320*200)
				cvZero( histimg );
			}

			cvCopy( frame, image, 0 );
			cvCvtColor( image, hsv, CV_BGR2HSV );			//convert the image color space; here, convert form RGB to HSV
		
			/* This is set to 0 initially.
			* Once the target is selected, it is reset to -1. 
			* Unless the object is in the window, it remains 1 
			*
			if( track_object )
			{
				int _vmin = vmin, _vmax = vmax;

				/* does the range check for every elemnt of the input array.
				* cvInRangeS(src, lower bound, upper bound, dest)
				*
				cvInRangeS( hsv, cvScalar(0,smin,MIN(_vmin,_vmax),0),
							cvScalar(180,256,MAX(_vmin,_vmax),0), mask );
				cvSplit( hsv, hue, 0, 0, 0 );		//split the source array(hsv) into different arrays

				/* track_object will be less than 0 only once,
				* wheenver the object is selected using mouse drag
				* after that, it is reset to 1
				*
				if( track_object < 0 )
				{
					float max_val = 0.f;
					cvSetImageROI( hue, selection );				//set the region of interest
					cvSetImageROI( mask, selection );
					cvCalcHist( &hue, hist, 0, mask );				//calculate the image histogram. 
																	//the range is 0-180 and the bin size = 16
		
					cvGetMinMaxHistValue( hist, 0, &max_val, 0, 0 );		//get the min and max histogram bins (here only max)
					cvConvertScale( hist->bins, hist->bins, max_val ? 255. / max_val : 0., 0 );
					cvResetImageROI( hue );
					cvResetImageROI( mask );
					track_window = selection;
					track_object = 1;

					cvZero( histimg );
					bin_w = histimg->width / hdims;			// hdims=16, calculate the bin width
					for( i = 0; i < hdims; i++ )
					{
						int val = cvRound( cvGetReal1D(hist->bins,i)*histimg->height/255 );
						CvScalar color = hsv2rgb(i*180.f/hdims);
						cvRectangle( histimg, cvPoint(i*bin_w,histimg->height),
									 cvPoint((i+1)*bin_w,histimg->height - val),
									 color, -1, 8, 0 );
					}
				}

				cvCalcBackProject( &hue, backproject, hist );		//calculate backprojection, which means 
																	//highlighting only the desired pixels. rest all are blackened

				cvAnd( backproject, mask, backproject, 0 );			//ANDing of the two images, backproject and mask.
																	//stored in backproject


				/* IMPLEMENTS THE CAMSHIFT ALGORITHM.
				cvCamShift(probImage, window, criteria)
				probImage: the backprojection of the image
				window: the initial search window
				criteria: the termination criteria
				*

				cvCamShift( backproject, track_window,
							cvTermCriteria( CV_TERMCRIT_EPS | CV_TERMCRIT_ITER, 10, 1 ),
							&track_comp, &track_box );
				track_window = track_comp.rect;
            
				if( backproject_mode )
					cvCvtColor( backproject, image, CV_GRAY2BGR );
				if( !image->origin )	
					track_box.angle = -track_box.angle;
				//draw the object ellipse
				cvEllipseBox( image, track_box, CV_RGB(0,255,0), 3, CV_AA, 0 );
			}
       	
			if( select_object && selection.width > 0 && selection.height > 0 )
			{
				cvSetImageROI( image, selection );
				cvXorS( image, cvScalarAll(255), image, 0 );
				cvResetImageROI( image );
			}


			//show the image (camera capture) and the histogram
			cvShowImage( "CamShiftDemo", image );
			cvShowImage( "Histogram", histimg );
		
			//wait for input.
			c = cvWaitKey(10);
			if( (char) c == 27 )
				break;
			switch( (char) c )
			{
			case 'b':
				backproject_mode ^= 1;
				break;
			case 'c':
				track_object = 0;
				cvZero( histimg );
				break;
			case 'h':
				show_hist ^= 1;
				if( !show_hist )
					cvDestroyWindow( "Histogram" );
				else
					cvNamedWindow( "Histogram", 1 );
				break;
			default:
				;
			}
		}

		cvReleaseCapture( &capture );
		cvDestroyWindow("CamShiftDemo");
		fcloseall();
		return 0;
	}
	catch(cv::Exception ex)
	{
		printf("%s",ex.what());
		getchar();
		return -1;
	}

}

#ifdef _EiC
main(1,"main.cpp");
#endif			*/