#include "_types.hpp"
#include "_startdx.hpp"
#include "material20.hpp"
#include "shadercompile.hpp"
#include "shadercodegen.hpp"
#include "rtmanager.hpp"
#include "engine.hpp" // <-- wuarghl

#include "material20_zfillvs.hpp"
#include "material20_zfillps.hpp"
#include "material20_texvs.hpp"
#include "material20_texps.hpp"
#include "material20_lightvs.hpp"
#include "material20_lightps.hpp"
#include "material20_fatvs.hpp"
#include "material20_fatps.hpp"
#include "material20_envivs.hpp"
#include "material20_envips.hpp"
#include "material20_vcolorvs.hpp"
#include "material20_vcolorps.hpp"

/*
 missing functionality:
 - paralax mapping
 - uv source parameter
 - envi doesn't do detail bump
 - envi doesn't do bump tex transform
*/

#if sLINK_MTRL20

/****************************************************************************/

sMaterial20Base::sMaterial20Base(const sMaterial20Para &para)
{
  Para = para;
  UseSRT = sFALSE;
  TexCount = 0;

  for(sInt i=0;i<8;i++)
  {
    Tex[i] = sINVALID;
    TexSet[i] = sINVALID;
  }

  for(sInt i=0;i<MaxNSetups;i++)
    Setup[i] = sINVALID;
}

sMaterial20Base::~sMaterial20Base()
{
  for(sInt i=0;i<8;i++)
  {
    if(Tex[i] != sINVALID)
      sSystem->RemTexture(Tex[i]);
  }

  for(sInt i=0;i<MaxNSetups;i++)
    if(Setup[i] != sINVALID)
      sSystem->MtrlRemSetup(Setup[i]);
}

void sMaterial20Base::DefaultStates(sU32 *&states,sBool alphaTest,sBool zFill,sBool stencilTest,sInt alphaBlend)
{
  sU32 *st = states;

  // render state setup
  *st++ = sD3DRS_ALPHATESTENABLE;     *st++ = alphaTest;
  *st++ = sD3DRS_ZENABLE;             *st++ = sD3DZB_TRUE;
  *st++ = sD3DRS_ZWRITEENABLE;        *st++ = zFill;
  *st++ = sD3DRS_ZFUNC;               *st++ = zFill ? sD3DCMP_LESSEQUAL : sD3DCMP_EQUAL;
  *st++ = sD3DRS_CULLMODE;            *st++ = sD3DCULL_CCW;
  *st++ = sD3DRS_COLORWRITEENABLE;    *st++ = zFill ? 0 : 15;
  *st++ = sD3DRS_SLOPESCALEDEPTHBIAS; *st++ = 0;
  *st++ = sD3DRS_DEPTHBIAS;           *st++ = 0;
  *st++ = sD3DRS_FOGENABLE;           *st++ = 0;
  *st++ = sD3DRS_STENCILENABLE;       *st++ = stencilTest/* || zFill*/;
  *st++ = sD3DRS_ALPHABLENDENABLE;    *st++ = alphaBlend != 0;

  if(alphaTest)
  {
    *st++ = sD3DRS_ALPHAFUNC;         *st++ = sD3DCMP_GREATER;
    *st++ = sD3DRS_ALPHAREF;          *st++ = 128;
  }

  if(stencilTest)
  {
    *st++ = sD3DRS_TWOSIDEDSTENCILMODE; *st++ = 0;
    *st++ = sD3DRS_STENCILFUNC;         *st++ = sD3DCMP_EQUAL;
    *st++ = sD3DRS_STENCILFAIL;         *st++ = sD3DSTENCILOP_KEEP;
    *st++ = sD3DRS_STENCILZFAIL;        *st++ = sD3DSTENCILOP_KEEP;
    *st++ = sD3DRS_STENCILPASS;         *st++ = sD3DSTENCILOP_KEEP;
  }
  /*else if(zFill)
  {
    *st++ = sD3DRS_TWOSIDEDSTENCILMODE; *st++ = 0;
    *st++ = sD3DRS_STENCILFUNC;         *st++ = sD3DCMP_ALWAYS;
    *st++ = sD3DRS_STENCILFAIL;         *st++ = sD3DSTENCILOP_INCRSAT;
    *st++ = sD3DRS_STENCILZFAIL;        *st++ = sD3DSTENCILOP_KEEP;
    *st++ = sD3DRS_STENCILPASS;         *st++ = sD3DSTENCILOP_INCRSAT;
  }*/

  switch(alphaBlend)
  {
  case 1:
    *st++ = sD3DRS_SRCBLEND;            *st++ = sD3DBLEND_ONE;
    *st++ = sD3DRS_DESTBLEND;           *st++ = sD3DBLEND_ONE;
    break;

  case 2:
    *st++ = sD3DRS_SRCBLEND;            *st++ = sD3DBLEND_ONE;
    *st++ = sD3DRS_DESTBLEND;           *st++ = sD3DBLEND_INVSRCCOLOR;
    break;

  case 3:
    *st++ = sD3DRS_SRCBLEND;            *st++ = sD3DBLEND_DESTCOLOR;
    *st++ = sD3DRS_DESTBLEND;           *st++ = sD3DBLEND_ZERO;
    break;
  }

  states = st;
}

void sMaterial20Base::AddSampler(sU32 *&states,sInt handle,sU32 flags)
{
  TexSet[TexCount] = handle;
  sU32 *st = states;
  sInt base = sD3DSAMP_1 * TexCount;

  // filter mode
  static const sU32 filters[8][3] = 
  {
    { sD3DTEXF_POINT,sD3DTEXF_POINT,sD3DTEXF_NONE },
    { sD3DTEXF_LINEAR,sD3DTEXF_LINEAR,sD3DTEXF_NONE },
    { sD3DTEXF_LINEAR,sD3DTEXF_LINEAR,sD3DTEXF_POINT },
    { sD3DTEXF_LINEAR,sD3DTEXF_LINEAR,sD3DTEXF_LINEAR },
    { sD3DTEXF_LINEAR,sD3DTEXF_ANISOTROPIC,sD3DTEXF_LINEAR },
    { sD3DTEXF_POINT,sD3DTEXF_POINT,sD3DTEXF_NONE },
    { sD3DTEXF_POINT,sD3DTEXF_POINT,sD3DTEXF_NONE },
    { sD3DTEXF_POINT,sD3DTEXF_POINT,sD3DTEXF_NONE },
  };

  *st++ = base|sD3DSAMP_MAGFILTER;  *st++ = filters[flags & 7][0];
  *st++ = base|sD3DSAMP_MINFILTER;  *st++ = filters[flags & 7][1];
  *st++ = base|sD3DSAMP_MIPFILTER;  *st++ = filters[flags & 7][2];

  // tile or clamp?
  sU32 addr = (flags & 0x100) ? sD3DTADDRESS_CLAMP : sD3DTADDRESS_WRAP;

  *st++ = base|sD3DSAMP_ADDRESSU;   *st++ = addr;
  *st++ = base|sD3DSAMP_ADDRESSV;   *st++ = addr;

  // done.
  states = st;

  TexCount++;
  if(((flags >> 12) & 0x0f) >= 2)
    UseSRT = sTRUE;
}

void sMaterial20Base::UseTex(sU32 *&states,sInt index,sInt *handles)
{
  sInt handle = handles[index];
  sU32 flags = (index >= 4) ? Para.LgtFlags[index - 4] : Para.TexFlags[index];
  sInt &used = (index >= 4) ? Para.LgtUsed[index - 4] : Para.TexUsed[index];

  Tex[index] = handle;
  if(Tex[index] != sINVALID)
  {
    sSystem->AddRefTexture(Tex[index]);
    AddSampler(states,handle,flags);
    used = sTRUE;
  }
  else
    used = sFALSE;
}

void sMaterial20Base::UpdatePara(const sMaterial20Para &para)
{
  Para = para;
}

void sMaterial20Base::Compile(sInt index,const sU32 *states,const sU32 *vs,const sU32 *ps)
{
  // compile shaders
  sShaderCodeGen *gen = new sShaderCodeGen;
  sU32 *vShader = gen->GenCode(vs,&Para.Flags);
  sU32 *pShader = gen->GenCode(ps,&Para.Flags);
  delete gen;

  // create setup
  if(vShader && pShader)
    Setup[index] = sSystem->MtrlAddSetup(states,vShader,pShader);
  else
    Setup[index] = sINVALID;

  // cleanup
  delete[] vShader;
  delete[] pShader;
}

void sMaterial20Base::TexTransformMat(sMatrix &mat)
{
  mat.InitSRT(Para.SRT1);
  mat.Trans4();

  sF32 fs,fc;
  sFSinCos(Para.SRT2[2],fs,fc);
  mat.k.Init( Para.SRT2[0]*fc,Para.SRT2[1]*fs,0,Para.SRT2[3]);
  mat.l.Init(-Para.SRT2[0]*fs,Para.SRT2[1]*fc,0,Para.SRT2[4]);
}

/****************************************************************************/

sMaterial20ZFill::sMaterial20ZFill(const sMaterial20Para &para,sInt *tex)
  : sMaterial20Base(para)
{
  sU32 states[256],*st=states;
  
  DefaultStates(st,tex[3] != sINVALID,sTRUE,sFALSE,sFALSE);
  UseTex(st,3,tex);

  // states done
  *st++ = ~0U;                            *st++ = ~0U;

  // compile
  Compile(0,states,g_material20_zfill_vsh,g_material20_zfill_psh);
}

void sMaterial20ZFill::Set(const sMaterialEnv &env)
{
  sMaterialInstance inst;
  sMatrix vc[2];
  sVector pc[1];

  if(Setup[0] == sINVALID)
    return;

  inst.NumTextures = TexCount;
  for(sInt i=0;i<TexCount;i++)
    inst.Textures[i] = TexSet[i];

  inst.VSConstants = &vc[0].i;
  inst.PSConstants = &pc[0];
  inst.NumPSConstants = 0;

  if(Para.Flags & 0x100) // alpha burn
  {
    pc[0].x = Para.ParalaxStrength;
    pc[0].y = 0.625f*0.625f;
    pc[0].z = 8.0f;
    pc[0].w = 0.0f;
    inst.NumPSConstants = 1;
  }

  // matrices
  vc[0].Mul4(env.ModelSpace,sSystem->LastViewProject);
  vc[0].Trans4();

  // texture transforms
  if(UseSRT)
  {
    TexTransformMat(vc[1]);
    inst.NumVSConstants = 8;
  }
  else
  {
    vc[1].i.x = Para.TexScale[3];
    inst.NumVSConstants = 5;
  }

  sSystem->MtrlSetSetup(Setup[0]);
  sSystem->MtrlSetInstance(inst);
}

/****************************************************************************/

sMaterial20Tex::sMaterial20Tex(const sMaterial20Para &para,sInt *tex,sBool useAmbient)
  : sMaterial20Base(para)
{
  sU32 states[256],*st=states;
  DefaultStates(st,sFALSE,sFALSE,sFALSE,0);

  for(sInt i=0;i<3;i++)
    UseTex(st,i,tex);

  // states done
  *st++ = ~0U;                            *st++ = ~0U;

  // prepare compiler data
  Para.SrcScale[0] = XS_X|X_C|4;
  Para.SrcScale[1] = XS_Y|X_C|4;
  Para.SrcScale[2] = XS_Z|X_C|4;
  Para.SrcScale[3] = XS_W|X_C|4;

  // compile
  Compile(0,states,g_material20_tex_vsh,g_material20_tex_psh);
  
  UseAmbient = useAmbient;
}

void sMaterial20Tex::Set(const sMaterialEnv &env)
{
  sMaterialInstance inst;
  sMatrix vc[3];
  sVector pc[4];

  if(Setup[0] == sINVALID)
    return;

  inst.NumTextures = TexCount;
  for(sInt i=0;i<TexCount;i++)
    inst.Textures[i] = TexSet[i];

  inst.VSConstants = &vc[0].i;

  sSystem->LastMatrix = env.ModelSpace;
  sSystem->LastMatrix.TransR();

  // ambient light level
  inst.PSConstants = &pc[0];
  inst.NumPSConstants = 1;
  if(UseAmbient)
    pc[0].InitColor(Engine->AmbientLight);
  else
    pc[0].Init(1,1,1,1);

  if(Para.Flags & 0x200) // main color correct
  {
    sVector gray;
    sMatrix m,m2;

    // hue shift (rotation around gray axis)
    gray.Init4(1,1,1,0);
    gray.Unit3();
    m.InitRot(gray,Para.ColC[0] * sPI2F);

    // saturation (contract towards gray)
    sF32 saturation = Para.ColC[1];
    static const sF32 r_weight = 0.3086f;
    static const sF32 g_weight = 0.6094f;
    static const sF32 b_weight = 0.0820f;
    m2.i.Init4(r_weight,r_weight,r_weight,0.0f);
    m2.i.Scale3(1.0f - saturation);
    m2.i.x += saturation;
    m2.j.Init4(g_weight,g_weight,g_weight,0.0f);
    m2.j.Scale3(1.0f - saturation);
    m2.j.y += saturation;
    m2.k.Init4(b_weight,b_weight,b_weight,0.0f);
    m2.k.Scale3(1.0f - saturation);
    m2.k.z += saturation;
    m2.l.Init4(0,0,0,1);
    m.MulA(m2);

    // contrast+brightness
    sF32 contrast = Para.ColC[2];
    m2.Init();
    m2.i.x = m2.j.y = m2.k.z = contrast;
    sF32 shift = 0.5f * (1.0f - contrast) + Para.ColC[3];
    m2.l.Init4(shift,shift,shift,1.0f);
    m.MulA(m2);

    // copy to pixel shader constants
    m.Trans4();
    pc[1] = m.i;
    pc[2] = m.j;
    pc[3] = m.k;

    inst.NumPSConstants = 4;
  }

  // world view projection matrix
  vc[0].Mul4(env.ModelSpace,sSystem->LastViewProject);
  vc[0].Trans4();

  // texture transforms
  for(sInt i=0;i<4;i++)
    vc[1].i[i] = Para.TexScale[i];

  inst.NumVSConstants = 5;

  // texture transform matrices
  if(UseSRT)
  {
    TexTransformMat(vc[2]);
    inst.NumVSConstants = 12;
  }

  sSystem->MtrlSetSetup(Setup[0]);
  sSystem->MtrlSetInstance(inst);
}

/****************************************************************************/

sMaterial20Light::sMaterial20Light(const sMaterial20Para &para,sInt *tex,sU32 texTarget)
  : sMaterial20Base(para)
{
  sU32 states[256],*st=states;

  DefaultStates(st,sFALSE,sFALSE,sTRUE,(para.Flags & 4) ? 2 : 1);

  // input render target
  TexTarget = texTarget;
  AddSampler(st,0,0x00000100);

  // bump maps
  for(sInt i=0;i<2;i++)
    UseTex(st,i+4,tex);

  // states done
  *st++ = ~0U;                            *st++ = ~0U;

  // prepare compiler data
  Para.SrcScale[0] = XS_X|X_C|11;
  Para.SrcScale[1] = XS_Y|X_C|11;
  Para.SrcScale[2] = XS_Z|X_C|11;
  Para.SrcScale[3] = XS_W|X_C|11;

  for(sInt ltype=0;ltype<4;ltype++)
  {
    Para.LgtType = (ltype & 1) | ((ltype & 2) << 7);
    Compile(ltype,states,g_material20_light_vsh,g_material20_light_psh);
  }
}

void sMaterial20Light::Set(const sMaterialEnv &env)
{
  sMaterialInstance inst;
  sMatrix vc[4],temp;
  sVector pc[2],lightColor;
  sInt lightType = env.LightType;
  sInt setupIndex = env.LightType | ((env.Flags & 1)*2);

  if(lightType<0 || lightType>sLT_COUNT || Setup[setupIndex] == sINVALID)
    return;

  // get the render target
  sF32 uScale,vScale;

  TexSet[0] = RenderTargetManager->Get(TexTarget,uScale,vScale);

  // set textures
  inst.NumTextures = TexCount;
  for(sInt i=0;i<TexCount;i++)
    inst.Textures[i] = TexSet[i];

  inst.VSConstants = &vc[0].i;
  inst.PSConstants = pc;
  inst.NumPSConstants = 2;

  sSystem->LastMatrix = env.ModelSpace;
  sSystem->LastMatrix.TransR();

  // world view projection matrix
  vc[0].Mul4(env.ModelSpace,sSystem->LastViewProject);

  // input texture matrix
  temp.Init();
  temp.i.x = 0.5f * uScale;
  temp.j.y = -0.5f * vScale;
  temp.k.z = 0.5f;
  temp.l.x = 0.5f * uScale + 0.5f / sSystem->ViewportX;
  temp.l.y = 0.5f * vScale + 0.5f / sSystem->ViewportY;
  vc[1].Mul4(vc[0],temp);

  // tranpose matrices
  vc[0].Trans4();
  vc[1].Trans4();

  // light position, camera position, attenuation
  sVector rotPos;
  rotPos.Rotate34(sSystem->LastMatrix,env.LightPos);

  if(lightType == sLT_DIRECTIONAL)
    vc[2].i.Rotate3(sSystem->LastMatrix,env.LightDir);
  else
    vc[2].i = rotPos;

  vc[2].j.Rotate34(sSystem->LastMatrix,env.CameraSpace.l);

  sF32 scalef = 1.0f / env.LightRange;
  vc[2].k.Scale4(rotPos,-scalef);
  vc[2].k.w = scalef;
  
  // texture transforms
  for(sInt i=0;i<4;i++)
    vc[2].l[i] = Para.LgtScale[i];

  inst.NumVSConstants = 12;

  // --- pixel shader constants
  lightColor = env.LightColor;
  lightColor.Scale4(env.LightAmplify);
  pc[0].InitColor(Para.Diffuse);
  pc[0].Mul4(lightColor);
  pc[1].InitColor(Para.Specular);
  pc[1].Mul4(lightColor);
  pc[1].w = Para.SpecularPow;

  if(Para.Flags & 0x40) // 2x intensity
  {
    pc[0].Scale4(2.0f);
    pc[1].Scale4(2.0f);
  }

  // texture transform matrices
  if(UseSRT)
  {
    TexTransformMat(vc[3]);
    inst.NumVSConstants = 16;
  }

  sSystem->MtrlSetSetup(Setup[setupIndex]);
  sSystem->MtrlSetInstance(inst);
}

/****************************************************************************/

sMaterial20Fat::sMaterial20Fat(const sMaterial20Para &para,sInt *tex)
  : sMaterial20Base(para)
{
  sU32 states[256],*st;

  st = states;
  DefaultStates(st,sFALSE,sFALSE,sTRUE,(para.Flags & 4) ? 2 : 1);
  for(sInt i=0;i<2;i++)
    UseTex(st,i+4,tex);

  for(sInt i=0;i<3;i++)
    UseTex(st,i,tex);

  // states done
  *st++ = ~0U;                            *st++ = ~0U;

  for(sInt ind=0;ind<4;ind++)
  {
    // prepare compiler data
    Para.LgtType = (ind&1) | ((ind&2)<<7);
    Para.SrcScale[0] = XS_X|X_C|7;
    Para.SrcScale[1] = XS_Y|X_C|7;
    Para.SrcScale[2] = XS_Z|X_C|7;
    Para.SrcScale[3] = XS_W|X_C|7;
    Para.SrcScale2[0] = XS_X|X_C|12;
    Para.SrcScale2[1] = XS_Y|X_C|12;
    Para.SrcScale2[2] = XS_Z|X_C|12;
    Para.SrcScale2[3] = XS_W|X_C|12;

    Compile(ind,states,g_material20_fat_vsh,g_material20_fat_psh);
  }
}

void sMaterial20Fat::Set(const sMaterialEnv &env)
{
  sMaterialInstance inst;
  sMatrix vc[4];
  sVector pc[2],lightColor;
  sInt lightType = env.LightType;
  sInt setupIdx = lightType;

  if(lightType<0 || lightType>sLT_COUNT || Setup[setupIdx] == sINVALID)
    return;

  // set textures
  inst.NumTextures = TexCount;
  for(sInt i=0;i<TexCount;i++)
    inst.Textures[i] = TexSet[i];

  inst.VSConstants = &vc[0].i;
  inst.PSConstants = pc;
  inst.NumPSConstants = 2;

  sSystem->LastMatrix = env.ModelSpace;
  sSystem->LastMatrix.TransR();

  // world view projection matrix
  vc[0].Mul4(env.ModelSpace,sSystem->LastViewProject);
  vc[0].Trans4();

  // light position, camera position, attenuation
  sVector rotPos;
  rotPos.Rotate34(sSystem->LastMatrix,env.LightPos);

  if(lightType == sLT_DIRECTIONAL)
    vc[1].i.Rotate3(sSystem->LastMatrix,env.LightDir);
  else
    vc[1].i = rotPos;

  vc[1].j.Rotate34(sSystem->LastMatrix,env.CameraSpace.l);
  
  sF32 scalef = 1.0f / env.LightRange;
  vc[1].k.Scale4(vc[1].i,-scalef);
  vc[1].k.w = scalef;

  // texture transforms
  for(sInt i=0;i<4;i++)
  {
    vc[1].l[i] = Para.TexScale[i];
    vc[3].i[i] = Para.LgtScale[i];
  }

  // texture transform matrices
  if(UseSRT)
    TexTransformMat(vc[2]);

  inst.NumVSConstants = 13;

  // --- pixel shader constants
  lightColor = env.LightColor;
  lightColor.Scale4(env.LightAmplify);
  pc[0].InitColor(Para.Diffuse);
  pc[0].Mul4(lightColor);
  pc[1].InitColor(Para.Specular);
  pc[1].Mul4(lightColor);
  pc[1].w = Para.SpecularPow;

  if(Para.Flags & 0x40) // 2x intensity
  {
    pc[0].Scale4(2.0f);
    pc[1].Scale4(2.0f);
  }

  sSystem->MtrlSetSetup(Setup[lightType]);
  sSystem->MtrlSetInstance(inst);
}

/****************************************************************************/

sMaterial20Envi::sMaterial20Envi(const sMaterial20Para &para,sInt *tex)
  : sMaterial20Base(para)
{
  sU32 states[256],*st=states;

  DefaultStates(st,sFALSE,sFALSE,sFALSE,1);

  // envi
  UseTex(st,6,tex);

  // bump (if required)
  if((para.LgtFlags[2] & 0x20000) || (para.Flags & 0x80))
    UseTex(st,4,tex);

  // states done
  *st++ = ~0U;                            *st++ = ~0U;

  Compile(0,states,g_material20_envi_vsh,g_material20_envi_psh);
}

void sMaterial20Envi::Set(const sMaterialEnv &env)
{
  sMaterialInstance inst;
  sMatrix vc[3];

  if(Setup[0] == sINVALID)
    return;

  // set textures
  inst.NumTextures = TexCount;
  for(sInt i=0;i<TexCount;i++)
    inst.Textures[i] = TexSet[i];

  inst.VSConstants = &vc[0].i;
  inst.NumPSConstants = 0;

  sSystem->LastMatrix = env.ModelSpace;
  sSystem->LastMatrix.TransR();

  // matrices
  vc[0].Mul4(env.ModelSpace,sSystem->LastViewProject);
  if(Para.LgtFlags[2] & 0x10000)
    vc[1] = env.ModelSpace;
  else
    vc[1].MulA(env.ModelSpace,sSystem->LastCamera);
  vc[0].Trans4();
  vc[1].Trans4();

  // camera position
  vc[2].i.Rotate34(sSystem->LastMatrix,env.CameraSpace.l);
  inst.NumVSConstants = 9;

  sSystem->MtrlSetSetup(Setup[0]);
  sSystem->MtrlSetInstance(inst);
}

/****************************************************************************/

sMaterial20VColor::sMaterial20VColor(const sMaterial20Para &para,sInt *tex)
  : sMaterial20Base(para)
{
  sU32 states[256],*st=states;

  DefaultStates(st,sFALSE,sFALSE,sFALSE,3);

  // states done
  *st++ = ~0U;                            *st++ = ~0U;

  Compile(0,states,g_material20_vcolor_vsh,g_material20_vcolor_psh);
}

void sMaterial20VColor::Set(const sMaterialEnv &env)
{
  sMaterialInstance inst;
  sMatrix vc[2];

  if(Setup[0] == sINVALID)
    return;

  // set textures
  inst.NumTextures = TexCount;
  for(sInt i=0;i<TexCount;i++)
    inst.Textures[i] = TexSet[i];

  inst.VSConstants = &vc[0].i;
  inst.NumPSConstants = 0;

  sSystem->LastMatrix = env.ModelSpace;
  sSystem->LastMatrix.TransR();

  // matrices
  vc[0].Mul4(env.ModelSpace,sSystem->LastViewProject);
  vc[0].Trans4();
  vc[1].i.InitColor(Engine->AmbientLight);
  //vc[1].i.Init((Para.Flags & 8) ? 1.0f : 0.0f,0,0,0);
  inst.NumVSConstants = 5;

  sSystem->MtrlSetSetup(Setup[0]);
  sSystem->MtrlSetInstance(inst);
}

/****************************************************************************/

#endif