// cleaner.cpp : Diese Datei enthält die Funktion "main". Hier beginnt und endet die Ausführung des Programms.
//

#include "pch.h"
#include <iostream>
#define cimg_use_jpeg
#include "CImg.h"
#include <queue>
#include "math.h"
#include "string"
#include "windows.h"
#define MAX_THREADS 2
using namespace cimg_library;

//structures
struct coord {
	int xc;
	int yc;
};
struct bubble {
	int x;
	int y;
	int min_x;
	int max_x;
	int min_y;
	int max_y;
};
typedef struct MyData {
	int val1;
} MYDATA, *PMYDATA;


//Functions
CImg<unsigned char> preprocessing(CImg<unsigned char> image);
void leveling();
void threading(std::vector<std::string> filevector);
DWORD WINAPI img_process(LPVOID lpParam);
void reg_recurse(int x,int y);
void white_recurse(int x, int y);
void whiten_bubble(int x, int y);
void whiten_text(int x, int y);
void corrections(CImg<unsigned char> source, int nr, int factor);
boolean is_text();
void color_green(int x, int y);
void color_white(int x, int y);
boolean check_corner(int x, int y);
std::vector<std::string> load();
coord get_pixel(CImg<unsigned char> source);
void test_pixel();
std::string ws2s(const std::wstring& s);

//Global Variables
CImgDisplay selection;
CImg<unsigned char> image;
CImg<unsigned char> final_image;
CImg<unsigned char> diff_mark_img;
std::vector<CImg<unsigned char>> ppimages;
std::vector<CImg<unsigned char>> final_images;
int done_images = 0;
std::vector<std::string> filevector;
float max_bubble_size;
float min_bubble_size;
int bubble_number = 0;
bubble changes[20];
int k = 0;
int min_y;
int min_x;
int max_y;
int max_x;
int limit;
int white_r = 255;
int white_g = 253;
int white_b = 255;
int spectrum;
boolean skipfill;
int filenumber;
std::string cur_dir;
std::vector<boolean> finished_threads;

std::queue<int> region_vectors_x;
std::queue<int> region_vectors_y;
int done_corrections = 0;
boolean mutex = true;

int main() {

	filevector = load();
	ppimages.resize(6);
	final_images.resize(6);
	boolean runs = false;
	
	for (int nr = 0; nr < filenumber && mutex; nr++) {

		std::string cur_file =  filevector.at(nr);
		const char const* c = cur_file.c_str();

		CImg<unsigned char> img(c);		//load image
		

		//std::cout << max_bubble_size;

		if (img.spectrum() <= 1) {									// if the spectrum is too small
			CImg<unsigned char> pre_copy(img.width(), img.height(), 1, 3);		// load image in pre_copy with spctrm = 3
			for (int x = 0; x < img.width() - 1; x++) {
				for (int y = 0; y < img.height() - 1; y++) {
					pre_copy(x, y, 0) = pre_copy(x, y, 1) = pre_copy(x, y, 2) = img(x, y, 0);
				}
			}
			final_images[nr%5] = pre_copy;									// and copy pre_copy to final image
		}
		else {
			final_images[nr%5] = img;
		}

		ppimages[nr%5] = preprocessing(final_images[nr%5]);										// do preprocessing on img and final_image
		done_images++;
		if (!runs) {
			threading(filevector);
			runs = true;
		}
		std::cout << "ci" << done_images;
		while (done_images - done_corrections >= 5) {	}
	}
	while(mutex){
	}
	return 0;
}

void threading(std::vector<std::string> filevector) {
	finished_threads.resize(filenumber);
	
	HANDLE  hThreadArray[MAX_THREADS];
	DWORD   dwThreadIdArray[MAX_THREADS];
	PMYDATA pDataArray[MAX_THREADS];

	
		
	pDataArray[1] = (PMYDATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
		sizeof(MYDATA));
	pDataArray[1]->val1 = 1;

	 CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		img_process,			 // thread function name
		pDataArray[1],          // argument to thread function 
		0,                      // use default creation flags 
		&dwThreadIdArray[1]);
	if (hThreadArray[1] == NULL)
		ExitProcess(3);
		
	return;
	
}

DWORD WINAPI img_process(LPVOID lpParam) {
	
	int i = 0;
	while (i < filenumber) {
		if (i < done_images) {
			image = ppimages[i%5];
			final_image = final_images[i%5];
			max_bubble_size = image.width() * image.height() * 0.07;
			min_bubble_size = max_bubble_size / 100;

			selection.display(final_image);
			int factor = final_image.height() / 1080 + 1;
			selection.resize(final_image.width() / factor, final_image.height() / factor);

			corrections(final_image,i%5,factor);

			std::string savelocation = cur_dir + "/results/final" + std::to_string(i) + ".jpg";
			std::cout << savelocation;
			const char const *sv = savelocation.c_str();
			final_image.save(sv);
			std::cout << "next image" << std::endl;
			i++;
			done_corrections++;
		}
	}
	
	mutex = false;
	return 0;	
}

CImg<unsigned char> preprocessing(CImg<unsigned char> image){

	image.blur_median(1, 0);
	int r, g, b, avg = 0;

	int test = 0;

	for (int x = 0; x < image.width()-1; x++) {		//flattening
		for (int y = 0; y < image.height()-1; y++) {
			r = image(x, y, 0);
			g = image(x, y, 1);
			b = image(x, y, 2);
			avg = (r + g + b) / 3;
			if (avg < 200) {						//flattening threshhold
				image(x, y, 0) = image(x, y, 1) = image(x, y, 2) = 0;
			}
			else {
				image(x, y, 0) = image(x, y, 1) = image(x, y, 2) = 255;
			}
		}
	}

	/*CImgDisplay main_displ(image, "output");
	while (!main_displ.is_closed())
		main_displ.wait();*/

	for (int y = 11; y < image.height() - 21; y++) {			//horizontal grouping
		for (int x = 11; x < image.width() - 30; x++) {
			for (int n = 0; n < 5; n++) {
				if (image(x + n, y, 0) == 0) {
					image(x, y, 0) = image(x, y, 1) = image(x, y, 2) = 0;
					n = 10;
				}
			}

		}
	}
	for (int x = 11; x < image.width() - 11; x++) {			//vertical grouping
		for (int y = 11; y < image.height() - 32; y++) {
			for (int n = 0; n < 5; n++) {
				if (image(x, y + n, 0) == 0) {
					image(x, y, 0) = image(x, y, 1) = image(x, y, 2) = 0;
					n = 30;
				}
			}

		}
	}

	//int newx;
	//int newy;

	//test_pixel();
	/*CImgDisplay main_displ(image, "output");
	main_displ.resize(image.width() / 2, image.height() / 2);
	while(true){}*/
	

	/*for (int x = 2; x < image.width() - 2; x++) {		//regioning
		//std::cout << "x ist " << x << std::endl;
		for (int y = 2; y < image.height() - 2; y++) {
			if (image(x, y, 0) == 0 && image(x, y, 1) != 1) {
				min_y = 4000;
				min_x = 4000;
				max_y = 0;
				max_x = 0;

				while (!region_vectors_x.empty()) {		//empty queues
					region_vectors_x.pop();
					region_vectors_y.pop();
				}
				k = 0;
				reg_recurse(x, y);						// fill queue from new point
				while (!region_vectors_x.empty()) {		// work off queue, fill new
					newx = region_vectors_x.front();	//this fillsthe region
					newy = region_vectors_y.front();
					region_vectors_x.pop();
					region_vectors_y.pop();
					reg_recurse(newx, newy);
				}
				while (!region_vectors_x.empty()) {		//empty queues
					region_vectors_x.pop();
					region_vectors_y.pop();
				}
				if (is_text()) {
					whiten_bubble(x, y);
				}
			}
		}
	}*/
	

	return image;
}

void leveling() {

	int x;
	int y;
	int black_threshhold = 0;
	int white_threshhold = 255;
	int r, g, b, avg;

	CImgDisplay thresh(final_image, "click left on a black and richt on a white pixel", 0, false, false);
	int factor = final_image.height() / 720;
	thresh.resize(final_image.width() / factor, final_image.height() / factor);
	while (!thresh.is_closed()) {
		if (thresh.button() & 1) {
			y = thresh.mouse_y()*factor;
			x = thresh.mouse_x()*factor;
			r = final_image(x, y, 0);
			g = final_image(x, y, 1);
			b = final_image(x, y, 2);
			black_threshhold = (r + g + b) / 3;
			std::cout << black_threshhold << std::endl;
		}
		if (thresh.button() & 2) {
			y = thresh.mouse_y()*factor;
			x = thresh.mouse_x()*factor;
			r = final_image(x, y, 0);
			g = final_image(x, y, 1);
			b = final_image(x, y, 2);
			white_threshhold = (r + g + b) / 3;
			std::cout << white_threshhold << std::endl;
		}
	}
	boolean done;
	for (int x = 0; x < final_image.width() - 1; x++) {		//leveling
		for (int y = 0; y < final_image.height() - 1; y++) {
			done = false;
			r = final_image(x, y, 0);
			g = final_image(x, y, 1);
			b = final_image(x, y, 2);
			avg = (r + g + b) / 3;

			/*if (avg <= black_threshhold) {						
				final_image(x, y, 0) = final_image(x, y, 1) = final_image(x, y, 2) = 0;
				done = true;
			}
			if( avg >= white_threshhold ) {
				final_image(x, y, 0) = final_image(x, y, 1) = final_image(x, y, 2) = 255;
				done = true;
			}*/

			if (r < black_threshhold)
				r = black_threshhold;
			if (g < black_threshhold)
				g = black_threshhold;
			if (b < black_threshhold)
				b = black_threshhold;

			if (r > white_threshhold)
				r = white_threshhold;
			if (g > white_threshhold)
				g = white_threshhold;
			if (b > white_threshhold)
				b = white_threshhold;

			double f = (255.0 / (white_threshhold - black_threshhold));
			final_image(x, y, 0) = (f * (r - black_threshhold));
			final_image(x, y, 1) = (f * (g - black_threshhold));
			final_image(x, y, 2) = (f * (b - black_threshhold));
			
		}
	}
	return;
}

void reg_recurse(int x, int y) {

	if (k > 20002)
		return;

	if (!image.containsXYZC(x + 1, y) || !image.containsXYZC(x - 1, y) || !image.containsXYZC(x, y + 1) || !image.containsXYZC(x, y - 1))
		return;
	k++;
	if (min_y == 4000) {
		min_y = max_y = y;
		max_x = min_x = x;
	}

	if (image(x, y, 0) == 0 && image(x, y, 1) != 1) {
		image(x, y, 1) = 1;
		image(x, y, 2) = 255;
	}
	if (image(x + 1, y, 0) == 0 && image(x + 1, y, 1) != 1) { // check black, not marked
		image(x + 1, y, 1) = 1;
		image(x + 1, y, 2) = 255;
		if (x + 1 > max_x)						// check possible maxima
			max_x = x + 1;
		region_vectors_x.push(x + 1);			// push pixel to queue
		region_vectors_y.push(y);
}
	

	if (image(x - 1, y, 0) == 0 && image(x - 1, y, 1) != 1) {
		image(x - 1, y, 1) = 1;
		image(x - 1, y, 2) = 255;
		if (x - 1 < min_x)
			min_x = x - 1;
		region_vectors_x.push(x - 1);
		region_vectors_y.push(y);
	}

	if (image(x, y + 1, 0) == 0 && image(x, y + 1, 1) != 1) {
		image(x, y + 1, 1) = 1;
		image(x, y + 1, 2) = 255;
		if (y + 1 > max_y)
			max_y = y + 1;
		region_vectors_x.push(x);
		region_vectors_y.push(y + 1);
	}

	if (image(x, y - 1, 0) == 0 && image(x, y - 1, 1) != 1) {
		image(x, y - 1, 1) = 1;
		image(x, y - 1, 2) = 255;
		if (y - 1 < min_y)
			min_y = y - 1;
		region_vectors_x.push(x);
		region_vectors_y.push(y - 1);
	}
	
	return;
}

boolean is_text() {
	if (k < 200 || k > 20000)
		return false;
	if (max_x - min_x > 200)
		return false;
	if (max_x == min_x)
		return false;
	if (0, 2 > ((max_y - min_y) / (max_x - min_x + 1)) || ((max_y - min_y) / (max_x - min_x + 1)) > 10)
		return false;
	if (k/(image.width()*image.height()) < 0.001 && k < 1000)
		return false;
	std::cout << "success" << std::endl;
	return true;
}

void white_recurse(int x, int y) {
	
	if (image(x, y, 0) == 0 && image(x, y, 1) == 1) {
		image(x, y, 0) = image(x, y, 1) = image(x, y, 2) = 255;
		final_image(x, y, 0) = final_image(x, y, 1) = final_image(x, y, 2) = 255;
	}
	
	if (image(x + 1, y, 0) == 0 && image(x + 1, y, 1) == 1) {
		image(x + 1, y, 0) = image(x + 1, y, 1) = image(x + 1, y, 2) = 255;
		final_image(x + 1, y, 0) = final_image(x + 1, y, 1) = final_image(x + 1, y, 2) = 255;
		color_green(x + 1, y);
		region_vectors_x.push(x + 1);
		region_vectors_y.push(y);	
	}
	
	if (image(x - 1, y, 0) == 0 && image(x - 1, y, 1) == 1) {
		image(x - 1, y, 0) = image(x - 1, y, 1) = image(x - 1, y, 2) = 255;
		final_image(x - 1, y, 0) = final_image(x - 1, y, 1) = final_image(x - 1, y, 2) = 255;	
		color_green(x - 1 , y);
		region_vectors_x.push(x - 1);
		region_vectors_y.push(y);
		
	}

	if (image(x, y + 1, 0) == 0 && image(x, y + 1, 1) == 1) {
		image(x, y + 1, 0) = image(x, y + 1, 1) = image(x, y + 1, 2) = 255;
		final_image(x, y + 1, 0) = final_image(x, y + 1, 1) = final_image(x, y + 1, 2) = 255;	
		color_green(x, y + 1);
		region_vectors_x.push(x);
		region_vectors_y.push(y + 1);
		
	}

	if (image(x, y - 1, 0) == 0 && image(x, y - 1, 1) == 1) {
		image(x, y - 1, 0) = image(x, y - 1, 1) = image(x, y - 1, 2) = 255;
		final_image(x, y - 1, 0) = final_image(x, y - 1, 1) = final_image(x, y - 1, 2) = 255;
		color_green(x, y - 1);
		region_vectors_x.push(x);
		region_vectors_y.push(y - 1);
	}

	return;
}

void whiten_bubble(int x, int y) {
	
	//if (250 < final_image(x, y, 1) < 255 && final_image(x, y, 0) == 255 && final_image(x, y, 2) == 255)
	//	return;

	int new_white_x;
	int new_white_y;
	int offset = 0;
	if (white_g == 251)
		white_g = 253;
	else
		white_g = 251;

	k = 0;
	
	for (int n = 0; n < image.width() - 1; n++) {		//run over and whiten bubble
		if (image(x + n, y, 0) == 255) {
			offset = n;
			n = image.width();
		}
	}
	min_y = 10000;
	min_x = 10000;
	max_y = 0;
	max_x = 0;

	while (!region_vectors_x.empty()) {		//empty queues
		region_vectors_x.pop();
		region_vectors_y.pop();
	}
	k = 0;
	color_white(x + offset, y);
	while (!region_vectors_x.empty()) {		// work off queue, fill new
		new_white_x = region_vectors_x.front();
		new_white_y = region_vectors_y.front();
		region_vectors_x.pop();
		region_vectors_y.pop();
		color_white(new_white_x, new_white_y);
	}
	if (k < min_bubble_size  || k > max_bubble_size)
		skipfill = true;

	if (skipfill) {
		skipfill = false;
		return;
	}
	std::cout << "step1";
	/*bubble b;
	b.x = x; b.y = y; b.max_x = max_x; b.max_y = y; b.min_x = min_x; b.min_y = min_y;
	changes[bubble_number] = b;
	bubble_number++;*/

	limit = max_y - min_y;
	if (max_x - min_x > limit)
		limit = max_x - min_x;

	whiten_text(x + offset, y);
	while (!region_vectors_x.empty()) {		// work off queue, fill new
		new_white_x = region_vectors_x.front();
		new_white_y = region_vectors_y.front();
		region_vectors_x.pop();
		region_vectors_y.pop();
		whiten_text(new_white_x, new_white_y);
	}
	return;
}

void whiten_text(int x, int y) {

	if (!image.containsXYZC(x + 1, y) || !image.containsXYZC(x - 1, y) || !image.containsXYZC(x, y + 1) || !image.containsXYZC(x, y - 1))
		return;

	if (image(x, y, 0) == white_g) {
		final_image(x, y, 0) = image(x, y, 0) = white_r;
		final_image(x, y, 1) = image(x, y, 1) = white_g + 1;
		final_image(x, y, 2) = image(x, y, 2) = white_b;
	}
	if (image(x + 1, y, 1) == white_g && image(x + 1, y, 1) != white_g + 1) { // check white, not marked
		final_image(x + 1, y, 0) = image(x + 1, y, 0) = white_r;
		final_image(x + 1, y, 1) = image(x + 1, y, 1) = white_g + 1;
		final_image(x + 1, y, 2) = image(x + 1, y, 2) = white_b;
		region_vectors_x.push(x + 1);			// push pixel to queue
		region_vectors_y.push(y);
	}
	if (image(x - 1, y, 1) == white_g && image(x - 1, y, 1) != white_g + 1) {
		final_image(x - 1, y, 0) = image(x - 1, y, 0) = white_r;
		final_image(x - 1, y, 1) = image(x - 1, y, 1) = white_g + 1;
		final_image(x - 1, y, 2) = image(x - 1, y, 2) = white_b;
		region_vectors_x.push(x - 1);
		region_vectors_y.push(y);
	}
	if (image(x, y + 1, 1) == white_g && image(x, y + 1, 1) != white_g + 1) {
		final_image(x, y + 1, 0) = image(x, y + 1, 0) = white_r;
		final_image(x, y + 1, 1) = image(x, y + 1, 1) = white_g + 1;
		final_image(x, y + 1, 2) = image(x, y + 1, 2) = white_b;
		region_vectors_x.push(x);
		region_vectors_y.push(y + 1);
	}
	if (image(x, y - 1, 1) == white_g && image(x, y - 1, 1) != white_g + 1) {
		final_image(x, y - 1, 0) = image(x, y - 1, 0) = white_r;
		final_image(x, y - 1, 1) = image(x, y - 1, 1) = white_g + 1;
		final_image(x, y - 1, 2) = image(x, y - 1, 2) = white_b;
		region_vectors_x.push(x);
		region_vectors_y.push(y - 1);
	}

	if (image(x + 1, y, 0) == 0) { // check black
		if (!check_corner(x + 1, y))
		for (int i = 1; i < max_x - min_x ; i++) {
			if (image.containsXYZC(x + i, y, 0)) {
				if (image(x + i, y, 1) == white_g || image(x + i, y, 1) == white_g + 1 && image(x + i, y, 0) == white_r && image(x + i, y, 2) == white_b) {
					for (int p = 0; p <= i; p++) {
						final_image(x + p, y, 0) = image(x + p, y, 0) = white_r;
						final_image(x + p, y, 1) = image(x + p, y, 1) = white_g;
						final_image(x + p, y, 2) = image(x + p, y, 2) = white_b;
						color_green(x + p, y);
					}
					i = max_x;
				}
			}
		}
	}
	
	return;
}

void corrections(CImg<unsigned char> source, int nr, int f) {
	boolean is_clicked = false;
	int x;
	int y;
	CImg<unsigned char> buffer;
	CImg<unsigned char> im_buffer;
	boolean once = true;

	int factor = source.height() / 720;
	selection.resize(source.width() / f, source.height() / f);
	const unsigned int key_seq = cimg::keySPACE;

	while (!selection.is_key(key_seq)) {		
		while (!is_clicked && !selection.is_key(key_seq)) {
			if (selection.button()&1) {
				std::cout  << "working";
				y = selection.mouse_y()*factor;
				x = selection.mouse_x()*factor;
				buffer = final_image;
				im_buffer = image;
				if(final_image.containsXYZC(x,y,0,0))
					whiten_bubble(x, y);
				is_clicked = true;
				selection.set_button();
				std::cout << "rdy" << std::endl;
			}
			if (selection.button() & 2) {
				final_image = buffer;
				image = im_buffer;
				is_clicked = true;
			}
			if (selection.button() & 4) {
				image = ppimages[nr];
				final_image = final_images[nr];
				is_clicked = true;
			}
			
			if (selection.is_key(cimg::keyL)) {
				leveling();
				std::cout << "leveled" << std::endl;
				selection.set_key();
				is_clicked = true;
			}
		}
		selection.display(final_image);
		selection.resize(source.width() / factor, source.height() / factor);
		is_clicked = false;
	}
	return;
}

void color_white(int x, int y) {

	k++;
	if (k > max_bubble_size) {
		skipfill = true;
		return;
	}

	if (min_y == 10000) {
		min_y = max_y = y;
		max_x = min_x = x;
	}

	if (!image.containsXYZC(x + 1, y) || !image.containsXYZC(x - 1, y) || !image.containsXYZC(x, y + 1) || !image.containsXYZC(x, y - 1))
		return;

	if (image(x, y, 0) == 255 && image(x, y, 1) != white_g) {
		image(x, y, 0) = white_r;
		image(x, y, 1) = white_g;
		image(x, y, 2) = white_b;
		color_green(x, y);

	}
	if (image(x + 1, y, 0) == 255 && image(x + 1, y, 1) != white_g) { // check white, not marked
		image(x + 1, y, 0) = white_r;
		image(x + 1, y, 1) = white_g;
		image(x + 1, y, 2) = white_b;
		if (x + 1 > max_x)
			max_x = x + 1;
		color_green(x + 1, y);
		region_vectors_x.push(x + 1);			// push pixel to queue
		region_vectors_y.push(y);
	}


	if (image(x - 1, y, 0) == 255 && image(x - 1, y, 1) != white_g) {
		image(x - 1, y, 0) = white_r;
		image(x - 1, y, 1) = white_g;
		image(x - 1, y, 2) = white_b;
		if (x - 1 < min_x)
			min_x = x - 1;
		color_green(x - 1, y);
		region_vectors_x.push(x - 1);
		region_vectors_y.push(y);
	}

	if (image(x, y + 1, 0) == 255 && image(x, y + 1, 1) != white_g) {
		image(x, y + 1, 0) = white_r;
		image(x, y + 1, 1) = white_g;
		image(x, y + 1, 2) = white_b;
		if (y + 1 > max_y)
			max_y = y + 1;
		color_green(x, y + 1);
		region_vectors_x.push(x);
		region_vectors_y.push(y + 1);
	}

	if (image(x, y - 1, 0) == 255 && image(x, y - 1, 1) != white_g) {
		image(x, y - 1, 0) = white_r;
		image(x, y - 1, 1) = white_g;
		image(x, y - 1, 2) = white_b;
		if (y - 1 < min_y)
			min_y = y - 1;
		color_green(x, y - 1);
		region_vectors_x.push(x);
		region_vectors_y.push(y - 1);
	}

	return;
}

boolean check_corner(int x, int y) {		// returns true if pixel is border between marked whited, non marked white
	boolean xup = false;
	boolean xdown = false;
	boolean yup = false;
	boolean ydown = false;
	boolean duu = false;
	boolean dud = false;
	boolean ddd = false;
	boolean ddu = false;
	
	
	for (int i = 0; i < limit; i++) {
		if (x + i < max_x - 2)
			if (image(x + i, y, 1) == white_g || image(x + i, y, 1) == white_g + 1)
				xup = true;
		if (x - i > min_x + 2)
			if (image(x - i, y, 1) == white_g || image(x - i, y, 1) == white_g + 1)
				xdown = true;
		if (y + i < max_y - 2)
			if (image(x, y + i, 1) == white_g || image(x, y + i, 1) == white_g + 1)
				yup = true;
		if (y - i > min_y + 2)
			if (image(x, y - i, 1) == white_g || image(x, y - i, 1) == white_g + 1)
				ydown = true;

		if (x + i < max_x - 2 && y + i < max_y - 2)
			if (image(x + i, y + i, 1) == white_g || image(x + i, y + i, 1) == white_g + 1)
				duu = true;
		if (x - i > min_x + 2 && y - i > min_y + 2)
			if (image(x - i, y - i, 1) == white_g || image(x - i, y - i, 1) == white_g + 1)
				ddd = true;
		if (y + i < max_y - 2 && x - i > min_x + 2)
			if (image(x - i, y + i, 1) == white_g || image(x - i, y + i, 1) == white_g + 1)
				ddu = true;
		if (y - i > min_y + 2 && x + i < max_x - 2)
			if (image(x + i, y - i, 1) == white_g || image(x + i, y - i, 1) == white_g + 1)
				dud = true;

	}
		
	return ( !(xup && xdown && yup && ydown && duu & ddd & dud & ddu));
}

void color_green(int x, int y) {
	/*diff_mark_img(x, y, 0) = 124;
	diff_mark_img(x, y, 1) = 252;
	diff_mark_img(x, y, 2) = 0;*/
	return;
}

std::vector<std::string> load() {
	
	filenumber = 0;

	TCHAR infoBuf[200];
	GetCurrentDirectory(200, infoBuf);
	std::wstring a(&infoBuf[0]);
	cur_dir = ws2s(a);
	cur_dir.resize(cur_dir.length() - 1);

	std::vector<std::string> files;
	std::wstring b = L"\\*.jpg";
	std::wstring ab = a + b;
	WIN32_FIND_DATA data;
	HANDLE hFind = FindFirstFile(ab.c_str(), &data);      // DIRECTORY

	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			char ch[260];
			char DefChar = ' ';
			WideCharToMultiByte(CP_ACP, 0, data.cFileName, -1, ch, 260, &DefChar, NULL);
			std::string file(ch);
			

			std::string cur_file = cur_dir + "/" + file;
			std::replace(cur_file.begin(), cur_file.end(), '\\', '/');
			files.push_back(cur_file);
			filenumber++;
		} while (FindNextFile(hFind, &data));
		FindClose(hFind);
	}
	return (files);
}

std::string ws2s(const std::wstring& s)
{
	int len;
	int slength = (int)s.length() + 1;
	len = WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, 0, 0, 0, 0);
	std::string r(len, '\0');
	WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, &r[0], len, 0, 0);
	return r;
}

coord get_pixel(CImg<unsigned char> source) {
	coord pixel;
	boolean is_clicked = false;
	CImgDisplay selection(source, "click a point",0,false,false);
	int factor = source.height() / 720;
	selection.resize(source.width()/factor,source.height()/factor);
	while (!is_clicked) {
		if (selection.button()) {
			pixel.yc = selection.mouse_y()*factor;
			pixel.xc = selection.mouse_x()*factor;
			is_clicked = true;
		}
	}
	//std::cout << "x is" << pixel.xc << " y is" << pixel.yc << std::endl;
	return pixel;
}

void test_pixel(){
	CImgDisplay mains(image, "debug");
	while (!mains.is_closed()){	
		coord t = get_pixel(image);
		std::cout << "limit is" << limit;
		std::cout << "minx_x is " << min_x <<"max_x is" << max_x << " min_y is " << min_y << "max_y is " << max_y << std::endl;
		if (check_corner(t.xc, t.yc))
			std::cout << "iscorner" << std::endl;
		else
			std::cout << std::endl;
	}
	return;
}

// Programm ausführen: STRG+F5 oder "Debuggen" > Menü "Ohne Debuggen starten"
// Programm debuggen: F5 oder "Debuggen" > Menü "Debuggen starten"

// Tipps für den Einstieg: 
//   1. Verwenden Sie das Projektmappen-Explorer-Fenster zum Hinzufügen/Verwalten von Dateien.
//   2. Verwenden Sie das Team Explorer-Fenster zum Herstellen einer Verbindung mit der Quellcodeverwaltung.
//   3. Verwenden Sie das Ausgabefenster, um die Buildausgabe und andere Nachrichten anzuzeigen.
//   4. Verwenden Sie das Fenster "Fehlerliste", um Fehler anzuzeigen.
//   5. Wechseln Sie zu "Projekt" > "Neues Element hinzufügen", um neue Codedateien zu erstellen, bzw. zu "Projekt" > "Vorhandenes Element hinzufügen", um dem Projekt vorhandene Codedateien hinzuzufügen.
//   6. Um dieses Projekt später erneut zu öffnen, wechseln Sie zu "Datei" > "Öffnen" > "Projekt", und wählen Sie die SLN-Datei aus.
