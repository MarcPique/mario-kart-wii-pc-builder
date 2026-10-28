// Persistent host settings used by the original game controls.
static int g_pcPreset=RuntimeConfigFile::Get().qualityPreset.value_or(5);
static uint32_t g_pcMsaa=RuntimeConfigFile::Get().msaaSamples.value_or(1);
static uint32_t g_pcAnisotropy=RuntimeConfigFile::Get().anisotropy.value_or(1);
static void PcSetMsaa(uint32_t value){g_pcMsaa=value;RuntimeConfigFile::Mutable().msaaSamples=value;RuntimeConfigFile::WriteSetting("video","msaa_samples",std::to_string(value));}
static void PcSetAnisotropy(uint32_t value){g_pcAnisotropy=value;RuntimeConfigFile::Mutable().anisotropy=value;RuntimeConfigFile::WriteSetting("video","anisotropy",std::to_string(value));}
static void PcMarkCustom(){g_pcPreset=5;RuntimeConfigFile::Mutable().qualityPreset=5;RuntimeConfigFile::WriteSetting("video","quality_preset","5");}
static void PcApplyPreset(int preset){
    GX_SYNC_INLINE();
    const float scales[]={1,1,2,3,4};const uint32_t filters[]={1,1,8,8,16};
    g_pcPreset=preset;SetResolutionScale(scales[preset]);PcSetMsaa(preset>=3?4:1);PcSetAnisotropy(filters[preset]);
    g_disableCopyFilter=preset!=0;aurora_set_disable_copy_filter(g_disableCopyFilter);RuntimeConfigFile::SetDisableCopyFilter(g_disableCopyFilter);
    g_disabledPostProcessingPaths=preset==1?0x10u:0u;RuntimeGameGraphicsOptions::SetDisabledPostProcessingPaths(g_disabledPostProcessingPaths);RuntimeConfigFile::SetDisabledPostProcessingPaths(g_disabledPostProcessingPaths);
    RuntimeConfigFile::Mutable().qualityPreset=preset;RuntimeConfigFile::WriteSetting("video","quality_preset",std::to_string(preset));
}
