#include "ocr_parser.h"
#include <tesseract/capi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Handle globale di Tesseract (Singleton pattern per C)
static TessBaseAPI* tess_handle = NULL;

int ocr_init(const char* language) {
    if (tess_handle != NULL) return 0; // Già inizializzato

    tess_handle = TessBaseAPICreate();
    if (!tess_handle) {
        fprintf(stderr, "[OCR] Impossibile creare l'istanza di Tesseract.\n");
        return -1;
    }

    // Inizializza con la lingua specificata (es. "ita" o "eng")
    if (TessBaseAPIInit3(tess_handle, NULL, language) != 0) {
        fprintf(stderr, "[OCR] Impossibile inizializzare Tesseract con la lingua: %s\n", language);
        TessBaseAPIDelete(tess_handle);
        tess_handle = NULL;
        return -1;
    }

    // PSM 3 = Fully automatic page segmentation, ma no OSD (più veloce e preciso per blocchi singoli)
    TessBaseAPISetPageSegMode(tess_handle, PSM_AUTO);
    
    return 0;
}

char* ocr_process_image(const unsigned char* image_data, int width, int height, int bytes_per_pixel, int bytes_per_line) {
    if (!tess_handle || !image_data) return NULL;

    // Passa i dati grezzi a Tesseract
    if (TessBaseAPISetImage(tess_handle, image_data, width, height, bytes_per_pixel, bytes_per_line) != 0) {
        return NULL;
    }

    // Esegui il riconoscimento
    if (TessBaseAPIRecognize(tess_handle, NULL) != 0) {
        return NULL;
    }

    // Ottieni il testo UTF-8 (Tesseract alloca questa stringa, dobbiamo copiarla per usare free() standard)
    char* tess_text = TessBaseAPIGetUTF8Text(tess_handle);
    if (!tess_text) return NULL;

    char* result = strdup(tess_text);
    TessDeleteText(tess_text); // Libera la memoria di Tesseract

    return result;
}

void ocr_cleanup(void) {
    if (tess_handle != NULL) {
        TessBaseAPIEnd(tess_handle);
        TessBaseAPIDelete(tess_handle);
        tess_handle = NULL;
    }
}