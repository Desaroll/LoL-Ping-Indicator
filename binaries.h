/// Mada Font.
extern char binary_Mada_ttf_start[];
extern char binary_Mada_ttf_end[];
extern char binary_Mada_ttf_size[];
const char* Mada_ttf_start = binary_Mada_ttf_start;
int Mada_ttf_size = binary_Mada_ttf_end-binary_Mada_ttf_start;

/// Loaded image.
extern char binary_loaded_png_start[];
extern char binary_loaded_png_end[];
extern char binary_loaded_png_size[];
const char* loaded_png_start = binary_loaded_png_start;
int loaded_png_size = binary_loaded_png_end-binary_loaded_png_start;

/// Loading image.
extern char binary_loading_png_start[];
extern char binary_loading_png_end[];
extern char binary_loading_png_size[];
const char* loading_png_start = binary_loading_png_start;
int loading_png_size = binary_loading_png_end-binary_loading_png_start;
