#include "latex_converter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h> 
#include <math.h>    


// ============================================================================
// TABELLA DI CONVERSIONE: Unicode → LaTeX
// ============================================================================
typedef struct {
    const char* unicode;
    const char* latex;
} symbol_map_t;

static const symbol_map_t symbol_table[] = {
    // Lettere greche
    {"α", "\\alpha"}, {"β", "\\beta"}, {"γ", "\\gamma"}, {"δ", "\\delta"},
    {"ε", "\\epsilon"}, {"ζ", "\\zeta"}, {"η", "\\eta"}, {"θ", "\\theta"},
    {"ι", "\\iota"}, {"κ", "\\kappa"}, {"λ", "\\lambda"}, {"μ", "\\mu"},
    {"ν", "\\nu"}, {"ξ", "\\xi"}, {"π", "\\pi"}, {"ρ", "\\rho"},
    {"σ", "\\sigma"}, {"τ", "\\tau"}, {"υ", "\\upsilon"}, {"φ", "\\phi"},
    {"χ", "\\chi"}, {"ψ", "\\psi"}, {"ω", "\\omega"},
    
    // Lettere greche maiuscole
    {"Α", "A"}, {"Β", "B"}, {"Γ", "\\Gamma"}, {"Δ", "\\Delta"},
    {"Ε", "E"}, {"Ζ", "Z"}, {"Η", "H"}, {"Θ", "\\Theta"},
    {"Ι", "I"}, {"Κ", "K"}, {"Λ", "\\Lambda"}, {"Μ", "M"},
    {"Ν", "N"}, {"Ξ", "\\Xi"}, {"Π", "\\Pi"}, {"Ρ", "P"},
    {"Σ", "\\Sigma"}, {"Τ", "T"}, {"Υ", "\\Upsilon"}, {"Φ", "\\Phi"},
    {"Χ", "X"}, {"Ψ", "\\Psi"}, {"Ω", "\\Omega"},
    
    // Operatori matematici
    {"±", "\\pm"}, {"∓", "\\mp"}, {"×", "\\times"}, {"÷", "\\div"},
    {"⋅", "\\cdot"}, {"∗", "\\ast"}, {"∘", "\\circ"}, {"⊕", "\\oplus"},
    {"⊗", "\\otimes"}, {"†", "\\dagger"}, {"‡", "\\ddagger"},
    
    // Relazioni
    {"≤", "\\leq"}, {"≥", "\\geq"}, {"≠", "\\neq"}, {"≈", "\\approx"},
    {"≡", "\\equiv"}, {"∼", "\\sim"}, {"≃", "\\simeq"}, {"≅", "\\cong"},
    {"∝", "\\propto"}, {"⊂", "\\subset"}, {"⊃", "\\supset"}, {"∈", "\\in"},
    {"∉", "\\notin"}, {"⊆", "\\subseteq"}, {"⊇", "\\supseteq"},
    
    // Simboli speciali
    {"∞", "\\infty"}, {"∂", "\\partial"}, {"∇", "\\nabla"}, {"∀", "\\forall"},
    {"∃", "\\exists"}, {"∅", "\\emptyset"}, {"ℵ", "\\aleph"},
    {"∑", "\\sum"}, {"∏", "\\prod"}, {"∫", "\\int"}, {"∮", "\\oint"},
    {"√", "\\sqrt"}, {"∛", "\\sqrt[3]"}, {"∜", "\\sqrt[4]"},
    
    // Frecce
    {"→", "\\rightarrow"}, {"←", "\\leftarrow"}, {"↔", "\\leftrightarrow"},
    {"⇒", "\\Rightarrow"}, {"⇐", "\\Leftarrow"}, {"⇔", "\\Leftrightarrow"},
    {"↑", "\\uparrow"}, {"↓", "\\downarrow"},
    
    // Delimitatori
    {"⟨", "\\langle"}, {"⟩", "\\rangle"}, {"⌊", "\\lfloor"}, {"⌋", "\\rfloor"},
    {"⌈", "\\lceil"}, {"⌉", "\\rceil"}, {"|", "\\vert"}, {"‖", "\\Vert"},
    
    // Altri simboli
    {"ℝ", "\\mathbb{R}"}, {"ℕ", "\\mathbb{N}"}, {"ℤ", "\\mathbb{Z}"},
    {"ℚ", "\\mathbb{Q}"}, {"ℂ", "\\mathbb{C}"},
    {"′", "'"}, {"″", "''"}, {"‴", "'''"},
    
    {NULL, NULL} // Terminatore
};

// unicode \to latex
char* convert_unicode_to_latex(const char* text) {
    if (!text) return NULL;
    
    // Stima dimensione buffer (peggior caso: ogni char diventa \command{})
    size_t max_len = strlen(text) * 10 + 1;
    char* result = malloc(max_len);
    if (!result) return NULL;
    
    char* out = result;
    const char* p = text;
    
    while (*p) {
        bool found = false;
        
        // Cerca il simbolo più lungo possibile (greedy matching)
        for (const symbol_map_t* map = symbol_table; map->unicode; map++) {
            size_t len = strlen(map->unicode);
            if (strncmp(p, map->unicode, len) == 0) {
                // Trovato! Copia il comando LaTeX
                size_t cmd_len = strlen(map->latex);
                memcpy(out, map->latex, cmd_len);
                out += cmd_len;
                p += len;
                found = true;
                break;
            }
        }
        
        if (!found) {
            // Carattere normale, copia così com'è
            *out++ = *p++;
        }
    }
    
    *out = '\0';
    return result;
}



// Helper: controlla se due blocchi sono verticalmente adiacenti (frazione)
static bool is_fraction_pattern(const raw_text_block_t* num, const raw_text_block_t* den) {
    if (!num || !den) return false;
    
    // Numeratore sopra, denominatore sotto
    float y_gap = den->bbox_y0 - num->bbox_y1;
    
    // Devono essere vicini verticalmente (gap < 15pt)
    if (y_gap < 0 || y_gap > 15.0f) return false;
    
    // Devono essere orizzontalmente allineati (stessa x0 ± 20pt)
    float x_diff = fabs(num->bbox_x0 - den->bbox_x0);
    if (x_diff > 20.0f) return false;
    
    // Entrambi devono essere relativamente corti (frazioni tipiche)
    if (strlen(num->content) > 30 || strlen(den->content) > 30) return false;
    
    return true;
}

// Helper: controlla se un blocco è un pedice/indice
static bool is_subscript_pattern(const raw_text_block_t* base, const raw_text_block_t* sub) {
    if (!base || !sub) return false;
    
    // Pedice sotto e a destra della base
    float y_diff = sub->bbox_y0 - base->bbox_y1;
    float x_pos = sub->bbox_x0 - base->bbox_x1;
    
    // Deve essere sotto (y_diff > 0) e vicino
    if (y_diff < 0 || y_diff > 10.0f) return false;
    
    // Deve essere a destra o sovrapposto
    if (x_pos < -10.0f || x_pos > 20.0f) return false;
    
    // Pedice deve essere più piccolo (altezza < 60% della base)
    float base_height = base->bbox_y1 - base->bbox_y0;
    float sub_height = sub->bbox_y1 - sub->bbox_y0;
    if (sub_height > base_height * 0.6f) return false;
    
    return true;
}

int detect_math_patterns(parsed_page_t* page) {
    if (!page || page->block_count < 2) return 0;
    
    // Marca blocchi già processati
    bool* processed = calloc(page->block_count, sizeof(bool));
    if (!processed) return -1;
    
    for (size_t i = 0; i < page->block_count; i++) {
        if (processed[i]) continue;
        
        raw_text_block_t* block = page->blocks[i];
        
        // Pattern 1: Frazioni (numeratore/denominatore)
        for (size_t j = i + 1; j < page->block_count; j++) {
            if (processed[j]) continue;
            
            raw_text_block_t* candidate = page->blocks[j];
            
            if (is_fraction_pattern(block, candidate)) {
                // Converti in frazione LaTeX
                char* num_latex = convert_unicode_to_latex(block->content);
                char* den_latex = convert_unicode_to_latex(candidate->content);
                
                char fraction[1024];
                snprintf(fraction, sizeof(fraction), "\\frac{%s}{%s}", 
                         num_latex ? num_latex : block->content,
                         den_latex ? den_latex : candidate->content);
                
                free(num_latex);
                free(den_latex);
                
                // Sostituisci contenuto del primo blocco
                free(block->content);
                block->content = strdup(fraction);
                block->type = BLOCK_TYPE_MATH;
                
                // Estendi bbox per coprire tutta la frazione
                if (candidate->bbox_x1 > block->bbox_x1) {
                    block->bbox_x1 = candidate->bbox_x1;
                }
                block->bbox_y1 = candidate->bbox_y1;
                
                // Marca secondo blocco come processato
                processed[j] = true;
                break;
            }
        }
        
        // Pattern 2: Pedici/Indici
        for (size_t j = i + 1; j < page->block_count; j++) {
            if (processed[j]) continue;
            
            raw_text_block_t* candidate = page->blocks[j];
            
            if (is_subscript_pattern(block, candidate)) {
                // Converti in pedice LaTeX
                char* base_latex = convert_unicode_to_latex(block->content);
                char* sub_latex = convert_unicode_to_latex(candidate->content);
                
                char subscript[512];
                snprintf(subscript, sizeof(subscript), "%s_{%s}", 
                         base_latex ? base_latex : block->content,
                         sub_latex ? sub_latex : candidate->content);
                
                free(base_latex);
                free(sub_latex);
                
                free(block->content);
                block->content = strdup(subscript);
                block->type = BLOCK_TYPE_MATH;
                
                processed[j] = true;
                break;
            }
        }
    }
    
    free(processed);
    return 0;
}

// ============================================================================
// FUNZIONE 3: Conversione Blocco → LaTeX
// ============================================================================
char* block_to_latex(const raw_text_block_t* block) {
    if (!block || !block->content) return NULL;
    
    // Controlla se il blocco contiene simboli matematici
    bool has_math = false;
    for (const symbol_map_t* map = symbol_table; map->unicode; map++) {
        if (strstr(block->content, map->unicode)) {
            has_math = true;
            break;
        }
    }
    
    if (!has_math) return NULL; // Non è una formula
    
    // Converti simboli Unicode
    char* latex = convert_unicode_to_latex(block->content);
    if (!latex) return NULL;
    
    // Wrappa in ambiente math se necessario
    size_t len = strlen(latex);
    char* wrapped = malloc(len + 10);
    if (!wrapped) {
        free(latex);
        return NULL;
    }
    
    snprintf(wrapped, len + 10, "$%s$", latex);
    free(latex);
    
    return wrapped;
}