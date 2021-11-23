# D3D12Samples

## Sample001
シンプルな3D矩形描画サンプルです。

This sample is simple rect rendering sample ins 3D view.

## Sample002
https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-147

Fast Fourier Transform(FFT)の複数回実行サンプルです。   
回数指定、実行キューの指定と、非同期コンピュートの場合は計算にかかったフレーム数を表示します。

This sample is multiple runs of Fast Fourier Transform (FFT).   
In this sample, you can specify the number of times and the execution queue.
In the case of asynchronous compute, it shows the number of frames it took to compute.

## Sample003
大量のトライアングル描画時の頂点フォーマットによるパフォーマンスの差異を見ることが出来ます。   
カラーフォーマットをU32とF32 RGBAとで変更することが出来ます。

You can see the performance difference between vertex formats when drawing a large number of triangles.   
You can change the color format between U32 and F32 RGBA.

## Sample004
頂点のローカル変換をコンピュートシェーダで行うサンプルです。   
法線情報をF16フォーマットとF32フォーマットで切り替えてパフォーマンスの差異を確認することが出来ます。

This sample is vertex local transformations in a compute shader.   
You can switch the normal format between F16 and F32 to see the difference in performance.

## Sample005
FFTを用いたブラーの効果を確認することができるサンプルです。   
XY軸方向のブラーの強さを変更することが出来ます。

This sample allows you to see the effect of blur using FFT.   
You can change the strength of the blur in the XY axis direction.

## Sample006
Sponzaのノーマルを表示するサンプルです。

This sample is to view normal in world space of Sponza model.

## Sample007
https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-148

リソースステートを管理するプロデューサーシステムの実装サンプルです。

This sample is implementation of a producer system that manages resource states.

## Sample008
https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-149

スクリーン空間平面リフレクションの実装サンプルです。

This sample is implementation of Screen Space Planar Reflection.

## Sample009
https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-156

DirectX Ray Tracingを用いたシンプルなメッシュ描画サンプルです。

This sample is implementation of simple mesh rendering using DirectX Ray Tracing (DXR).

## Sample010
https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-157

DXRを用いた"Ray Tracing in a weekend"の実装サンプルです。

This sample is implementation of "Ray Tracing in a weekend" using DXR.

## Sample011
https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-158   
https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-160

DXRを用いてglTFフォーマットのSponzaモデルを描画するサンプルです。   
直接光、間接光、シャドウの計算を行っています。

This sample is to draw Sponza model of glTF formats using DXR.   
Calculating direct light, indirect light, and shadows.

## Sample012
https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-159

DXRを用いてレイトレーシングとラスタライザによるハイブリッドレンダリングを実装しています。   
DXRではAOとシャドウをレンダリングしています。

This sample is implementation of hybrid rendering with ray tracing and rasterizer using DXR.   
Calculating AO and shadows using DXR.

## Smaple013
https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-161

DXRを用いた頂点単位のライトベイクを実装しています。

This sample is implementation of vertex light baker using DXR.

## Sample014
https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-162

DXRを用いたライトマップベイクを実装しています。

This sample is implementation fo light map baker using DXR.

## Sample015
https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-164

DXRによるAOとシャドウの簡単なデノイズサンプルです。

This sample is simple denoiser for AO and shadows.

## Sample016
https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-165   
https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-166

コンピュートシェーダによるMulti Draw Indirectの実装サンプルです。

This sample is implementation of Multi Draw Indirect using Compute Shader.

## Sample017
テクスチャフィルタリングの比較用サンプルです。   
Trilinear, Anosotropic, Grid Super Sampleの比較が可能です。

This is a sample for texture filtering comparison.   
You can compare Trilinear, Anosotropic, and Grid Super Sample.

## Sample018
独自メッシュリソースの読み込みを行うサンプルです。

This sample is loading original mesh format.

## Sample019
https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-167

DXRを用いてメッシュのSigned Distance Fieldを生成するサンプルです。

This sample is generating Signed Distance Field of mesh using DXR.

## Sample020
https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-168

ピクセル単位のVariable Rate Shadingを実装しています。

This sample is implementation of Variable Rate Shading per pixel.

## Sample021
DXRによるGlobal Illuminationの実装サンプルです。

This sample is implementation of global illumination using DXR.

## Sample022
https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-170

このサンプルではNVIDIA RTXGIを使用しています。

This sample is using NVIDIA RTXGI.

## Sample023
https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-169

OpenColorIOをGPUで実行するサンプルです。

This sample is execute OpenColorIO (OCIO) using GPUs.

## Sample024
https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-171   
https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-172

メッシュシェーダとアンプリフィケーションシェーダを用いたメッシュレットカリングを実装しています。

This sample is implementation of meshlet culling using Mesh Shader and Amplification Shader.

## Sample025
https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-173

バインドレステクスチャを用いたDeferred Textureの実装サンプルです。

This sample is implementation of deferred texture rendering using bindless textures.


## Sample026
https://sites.google.com/site/monshonosuana/directx%E3%81%AE%E8%A9%B1/directx%E3%81%AE%E8%A9%B1-%E7%AC%AC177%E5%9B%9E

コンピュートシェーダを用いた2フェーズ遮蔽カリングの実装サンプルです。

This sample is implementation of 2-phase occlusion culling using compute shader.

## Sample027

Fidelity-FX Super ResolutionとNVIDIA Image Scalingの実装サンプルです。

This sample is implementation of Fidelity-FX Super Resolution (FSR) and NVIDIA Image Scaling (NIS).
