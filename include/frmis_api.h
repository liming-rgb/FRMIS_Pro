#pragma once

#ifdef FRMIS_STATIC
    #define FRMIS_API
#elif defined(_WIN32)
    #ifdef frmis_EXPORTS
        #define FRMIS_API __declspec(dllexport)
    #else
        #define FRMIS_API __declspec(dllimport)
    #endif
#else
    #define FRMIS_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

    struct StitchingParams {
        const char* dataset_dir;
        const char* dataset_name;      // prefix of files, e.g. "snapshot_" or ""
        const char* img_type;          // extension, e.g. ".tif" or ".tiff"
        const char* modality;          // e.g. "BrightField", "phase&Fluorescent", or "customize"
        const char* optimization;      // "True" or "False"
        const char* global_registration; // "MST" or "SPT"
        const char* blend_method;      // "Overlay" or "Linear"
        
        int width;                     // column count
        int height;                    // row count
        int img_num;                   // total images (End)
        int sort_type;                 // 1 or 2
        int threshold_metric;          // SURF threshold
        
        float overlap_w;               // West overlap in percent (0-100)
        float overlap_h;               // North overlap in percent (0-100)
        float overlap_error;           // allowed deviation in percent (0-100)
        float alpha;                   // linear blend parameter
        
        int use_fast_stitch;           // 1=true, 0=false
        int use_surf;                  // 1=true, 0=false
        int use_pc;                    // 1=true, 0=false
    };

    FRMIS_API int RunStitchingPipeline(const StitchingParams* params);

#ifdef __cplusplus
}
#endif
