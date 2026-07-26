#ifndef EXAMPLES_H
#define EXAMPLES_H

#include <QByteArray>

namespace Examples {
// Basics
QByteArray jsonBasicsHello();
QByteArray jsonBasicsWarm();
QByteArray jsonBasicsMeshObj();
// Effects
QByteArray jsonEffectsFire();
QByteArray jsonEffectsPlasma();
QByteArray jsonEffectsWaterRipples();
QByteArray jsonEffectsStarfield();
QByteArray jsonEffectsAurora();
QByteArray jsonEffectsLavaLamp();
// Lighting
QByteArray jsonLightingToon();
QByteArray jsonLightingPhong();
QByteArray jsonLightingRimLight();
QByteArray jsonLightingMatcap();
QByteArray jsonLightingWireframe();
QByteArray jsonLightingNormals();
QByteArray jsonLightingDynamicPointLight();
QByteArray jsonLightingOrbitLight();
QByteArray jsonLightingMultiLight();
// Textures
QByteArray jsonTexturesTexturedQuad();
QByteArray jsonTexturesDistortion();
QByteArray jsonTexturesNormalMap();
QByteArray jsonTexturesMultiBlend();
// Post-processing
QByteArray jsonPostprocessCrt();
QByteArray jsonPostprocessPixelation();
QByteArray jsonPostprocessChromaticAberration();
QByteArray jsonPostprocessVignette();
QByteArray jsonPostprocessEdgeDetection();
QByteArray jsonPostprocessGlitch();
// Patterns
QByteArray jsonPatternsMandelbrot();
QByteArray jsonPatternsVoronoi();
QByteArray jsonPatternsTruchet();
QByteArray jsonPatternsSdfRaymarch();
QByteArray jsonPatternsKaleidoscope();
QByteArray jsonPatternsMoire();
// Multipass
QByteArray jsonMultipassBloom();
QByteArray jsonMultipassDeferred();
QByteArray jsonMultipassShadowMap();
QByteArray jsonMultipassEdgeGlow();
// Blending
QByteArray jsonBlendAdditiveGlow();
QByteArray jsonBlendTransparentLayers();
// Compute
QByteArray jsonComputeGameOfLife();
QByteArray jsonComputeFluidSim();
QByteArray jsonComputeImageBlur();
// Tessellation
QByteArray jsonTessellationDisplacement();
QByteArray jsonTessellationPnTriangles();
QByteArray jsonTessellationAdaptiveLod();
// Ray tracing
QByteArray jsonRtMinimalCompileTest();
QByteArray jsonRtReflections();
QByteArray jsonRtShadows();
QByteArray jsonRtProceduralGeo();
QByteArray jsonRtCallableMaterials();
QByteArray jsonRtAnyhitTransparency();
// Subgroup / Wave
QByteArray jsonSubgroupWaveReduction();
QByteArray jsonSubgroupWavePrefixScan();
} // namespace Examples

#endif
