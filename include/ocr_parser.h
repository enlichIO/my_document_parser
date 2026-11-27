#ifndef OCR_PARSER_H
#define OCR_PARSER_H

int ocr_init(const char* language);

char* ocr_process_image(const unsigned char* image_data, int width, int height, int bytes_per_pixel, int bytes_per_line);


void ocr_cleanup(void);

#endif /* OCR_PARSER_H */