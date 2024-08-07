// The MIT License (MIT)
//
// Copyright (c) 2013 Dan Ginsburg, Budirijanto Purnomo
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

//
// Book:      OpenGL(R) ES 3.0 Programming Guide, 2nd Edition
// Authors:   Dan Ginsburg, Budirijanto Purnomo, Dave Shreiner, Aaftab Munshi
// ISBN-10:   0-321-93388-5
// ISBN-13:   978-0-321-93388-1
// Publisher: Addison-Wesley Professional
// URLs:      http://www.opengles-book.com
//            http://my.safaribooksonline.com/book/animation-and-3d/9780133440133
//
// Shadows.c
//
//    Demonstrates shadow rendering with depth texture and 6x6 PCF
//
#include <stdlib.h>
#include <math.h>
#include "esUtil.h"

#include <stdio.h>

#define POSITION_LOC    0
#define COLOR_LOC       1

typedef struct
{
   // Handle to a program object
   GLuint sceneProgramObject;
   GLuint shadowMapProgramObject;

   // Uniform locations
   GLint  sceneMvpLoc;
   GLint  shadowMapMvpLoc;
   GLint  sceneMvpLightLoc;
   GLint  shadowMapMvpLightLoc;

   // Sampler location
   GLint shadowMapSamplerLoc;

   // shadow map Texture handle
   GLuint shadowMapTextureId;
   GLuint shadowMapBufferId;
   GLuint shadowMapTextureWidth;
   GLuint shadowMapTextureHeight;

   //test frame buffer
   GLuint test_framebufferId;
   GLuint testTextureId;
   GLuint screen_shader_ID_;
   GLuint screen_quadVAO_;

   int msaa_level_;

   // VBOs of the model
   GLuint groundPositionVBO;
   GLuint groundIndicesIBO;
   GLuint cubePositionVBO;
   GLuint cubeIndicesIBO;
   
   // Number of indices
   int    groundNumIndices;
   int    cubeNumIndices;

   // dimension of grid
   int    groundGridSize;

   // MVP matrices
   ESMatrix  groundMvpMatrix;
   ESMatrix  groundMvpLightMatrix;
   ESMatrix  cubeMvpMatrix;
   ESMatrix  cubeMvpLightMatrix;

   float eyePosition[3];
   float lightPosition[3];
} UserData;


#define ENABLE_GL_CHECK 1
unsigned int InnerCheckGLError(const char* file, int line);

#ifdef  ENABLE_GL_CHECK
#define CheckGLError(glFunc) \
    glFunc;\
    InnerCheckGLError(__FILE__, __LINE__);
#pragma   message("defined gl check")
#else
#define CheckGLError(glFunc) glFunc;

#endif

const char *ErrorDescription (GLenum flag)
{
  static char str[256] = {'\0'};

  switch (flag)
  {
    case GL_NO_ERROR:
      snprintf (str, sizeof (str), "0x%04x:%s", flag, "GL_NO_ERROR");
      break;
    case GL_INVALID_ENUM:
      snprintf (str, sizeof (str), "0x%04x:%s", flag, "GL_INVALID_ENUM");
      break;
    case GL_INVALID_VALUE:
      snprintf (str, sizeof (str), "0x%04x:%s", flag, "GL_INVALID_VALUE");
      break;
    case GL_INVALID_OPERATION:
      snprintf (str, sizeof (str), "0x%04x:%s", flag, "GL_INVALID_OPERATION");
      break;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      snprintf (str, sizeof (str), "0x%04x:%s", flag, "GL_INVALID_FRAMEBUFFER_OPERATION");
      break;
    case GL_OUT_OF_MEMORY:
      snprintf (str, sizeof (str), "0x%04x:%s", flag, "GL_OUT_OF_MEMORY");
      break;
    default:
      snprintf (str, sizeof (str), "unknown flag:0x%04x", flag);
    }

    return str;
}

unsigned int InnerCheckGLError(const char* file, int line)
{
        GLenum error_code = glGetError();
    if (error_code != GL_NO_ERROR)
      printf("error:%s, line:%d, des:%s\n", file, line, ErrorDescription(error_code));
      //   printf("ERROR:" << file << ":" << line << " ==>" << ErrorDescription(error_code) <<  std::endl;
    // else
    //    printf("ERROR:" << file << ":" << line << " ==>" << ErrorDescription(error_code) <<  std::endl;
    return error_code;
}


///
// Initialize the MVP matrix
//
int InitMVP ( ESContext *esContext )
{
   ESMatrix perspective;
   ESMatrix ortho;
   ESMatrix modelview;
   ESMatrix model;
   ESMatrix view;
   float    aspect;
   UserData *userData = (UserData *)esContext->userData;
   
   // Compute the window aspect ratio
   aspect = (GLfloat) esContext->width / (GLfloat) esContext->height;
   
   // Generate a perspective matrix with a 45 degree FOV for the scene rendering
   esMatrixLoadIdentity ( &perspective );
   esPerspective ( &perspective, 45.0f, aspect, 0.1f, 100.0f );

   // Generate an orthographic projection matrix for the shadow map rendering
   esMatrixLoadIdentity ( &ortho );
   esOrtho ( &ortho, -10, 10, -10, 10, -30, 30 );

   // GROUND
   // Generate a model view matrix to rotate/translate the ground
   esMatrixLoadIdentity ( &model );

   // Center the ground
   esTranslate ( &model, -2.0f, -2.0f, 0.0f );
   esScale ( &model, 10.0f, 10.0f, 10.0f );
   esRotate ( &model, 90.0f, 1.0f, 0.0f, 0.0f );

   // create view matrix transformation from the eye position
   esMatrixLookAt ( &view, 
                    userData->eyePosition[0], userData->eyePosition[1], userData->eyePosition[2],
                    0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f );

   esMatrixMultiply ( &modelview, &model, &view );

   // Compute the final ground MVP for the scene rendering by multiplying the 
   // modelview and perspective matrices together
   esMatrixMultiply ( &userData->groundMvpMatrix, &modelview, &perspective );

   // create view matrix transformation from the light position
   esMatrixLookAt ( &view, 
                    userData->lightPosition[0], userData->lightPosition[1], userData->lightPosition[2],
                    0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f );

   esMatrixMultiply ( &modelview, &model, &view );

   // Compute the final ground MVP for the shadow map rendering by multiplying the 
   // modelview and ortho matrices together
   esMatrixMultiply ( &userData->groundMvpLightMatrix, &modelview, &ortho );

   // CUBE
   // position the cube
   esMatrixLoadIdentity ( &model );
   esTranslate ( &model, 5.0f, -0.4f, -3.0f );
   esScale ( &model, 1.0f, 2.5f, 1.0f );
   esRotate ( &model, -15.0f, 0.0f, 1.0f, 0.0f );

   // create view matrix transformation from the eye position
   esMatrixLookAt ( &view, 
                    userData->eyePosition[0], userData->eyePosition[1], userData->eyePosition[2],
                    0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f );

   esMatrixMultiply ( &modelview, &model, &view );
   
   // Compute the final cube MVP for scene rendering by multiplying the 
   // modelview and perspective matrices together
   esMatrixMultiply ( &userData->cubeMvpMatrix, &modelview, &perspective );

   // create view matrix transformation from the light position
   esMatrixLookAt ( &view, 
                    userData->lightPosition[0], userData->lightPosition[1], userData->lightPosition[2],
                    0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f );

   esMatrixMultiply ( &modelview, &model, &view );
   
   // Compute the final cube MVP for shadow map rendering by multiplying the 
   // modelview and ortho matrices together
   esMatrixMultiply ( &userData->cubeMvpLightMatrix, &modelview, &ortho );

   return TRUE;
}

int InitShadowMap ( ESContext *esContext )
{
   GLint defaultFramebuffer = 0;
   glGetIntegerv ( GL_FRAMEBUFFER_BINDING, &defaultFramebuffer );
   UserData *userData =(UserData *) esContext->userData;
   GLenum none = GL_NONE;
   

   // use 1K by 1K texture for shadow map
   userData->shadowMapTextureWidth = 2560;
   userData->shadowMapTextureHeight = 1392;

#if 0
   {

      glGenTextures ( 1, &userData->testTextureId );
      glBindTexture ( GL_TEXTURE_2D, userData->testTextureId );
      glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
      glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
      glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE );
      glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE );
      InnerCheckGLError(__FILE__, __LINE__);
      // Setup hardware comparison
      // glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE );
      // glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL );

      glTexImage2D ( GL_TEXTURE_2D, 0, GL_RGBA,
               userData->shadowMapTextureWidth, userData->shadowMapTextureHeight, 
               0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
               InnerCheckGLError(__FILE__, __LINE__);

      glBindTexture ( GL_TEXTURE_2D, 0 );

      InnerCheckGLError(__FILE__, __LINE__);
      // setup fbo
      glGenFramebuffers ( 1, &userData->test_framebufferId );
      glBindFramebuffer ( GL_FRAMEBUFFER, userData->test_framebufferId );
      InnerCheckGLError(__FILE__, __LINE__);

      glFramebufferTexture2D ( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, userData->testTextureId, 0 );
      InnerCheckGLError(__FILE__, __LINE__);

      int ret = glCheckFramebufferStatus ( GL_FRAMEBUFFER );
      if ( GL_FRAMEBUFFER_COMPLETE !=  ret)
      {
         printf("create framebuff failed--1.  %x\n", ret);
         return FALSE;
      }
   }
#else
   {

      glGenTextures ( 1, &userData->testTextureId );
      glBindTexture ( GL_TEXTURE_2D_MULTISAMPLE, userData->testTextureId );
      InnerCheckGLError(__FILE__, __LINE__);
      // Setup hardware comparison
      // glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE );
      // glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL );

      // glTexImage2D ( GL_TEXTURE_2D, 0, GL_RGBA,
      //          userData->shadowMapTextureWidth, userData->shadowMapTextureHeight, 
      //          0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
      printf("userData->msaa_level_ :%d\n" , userData->msaa_level_ );
      glTexStorage2DMultisample( GL_TEXTURE_2D_MULTISAMPLE, userData->msaa_level_, GL_RGBA8, userData->shadowMapTextureWidth, userData->shadowMapTextureHeight, GL_TRUE);
               InnerCheckGLError(__FILE__, __LINE__);

      glBindTexture ( GL_TEXTURE_2D, 0 );

      InnerCheckGLError(__FILE__, __LINE__);
      // setup fbo
      glGenFramebuffers ( 1, &userData->test_framebufferId );
      glBindFramebuffer ( GL_FRAMEBUFFER, userData->test_framebufferId );
      InnerCheckGLError(__FILE__, __LINE__);

      glFramebufferTexture2D ( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, userData->testTextureId, 0 );
      InnerCheckGLError(__FILE__, __LINE__);


      {

         CheckGLError("StitchingRenderer::InitScreenFrameBuffer");
      //   create a renderbuffer object for depth and stencil attachment (we won't be sampling these)
        // create a renderbuffer object for depth and stencil attachment (we won't be sampling these)
        unsigned int rbo;
        CheckGLError(glGenRenderbuffers(1, &rbo));
        CheckGLError(glBindRenderbuffer(GL_RENDERBUFFER, rbo));

        CheckGLError(glRenderbufferStorageMultisample(GL_RENDERBUFFER, userData->msaa_level_, GL_DEPTH24_STENCIL8, userData->shadowMapTextureWidth, userData->shadowMapTextureHeight));
      //   CheckGLError(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, avm_window_width_, avm_window_height_)); // use a single renderbuffer object for both a depth AND stencil buffer.
        CheckGLError(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo)); // now actually attach it

      }

      int ret = glCheckFramebufferStatus ( GL_FRAMEBUFFER );

   int status = ret;
      switch (status)
    {
    case GL_FRAMEBUFFER_COMPLETE:
        {
            printf("Success::FRAMEBUFFER:: Framebuffer is  complete(GL_FRAMEBUFFER_COMPLETE)! ");
        }
        break;
    case GL_FRAMEBUFFER_UNDEFINED:
        {
            printf("ERROR::FRAMEBUFFER:: Framebuffer is not complete(GL_FRAMEBUFFER_UNDEFINED)! ");
        }
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        {
            printf("ERROR::FRAMEBUFFER:: Framebuffer is not complete(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)! ");
        }
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        {
            printf("ERROR::FRAMEBUFFER:: Framebuffer is not complete(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)! ");
        }
        break;
    case GL_FRAMEBUFFER_UNSUPPORTED:
        {
            printf("ERROR::FRAMEBUFFER:: Framebuffer is not complete(GL_FRAMEBUFFER_UNSUPPORTED)! ");
        }
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        {
            printf("ERROR::FRAMEBUFFER:: Framebuffer is not complete(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)! ");
        }
        break;
    default:
        printf("ERROR::FRAMEBUFFER:: Framebuffer is not complete(UNKNOW)! %x" , status );
        break;
    }

      if ( GL_FRAMEBUFFER_COMPLETE !=  ret)
      {
         printf("create framebuff failed--1.  %x\n", ret);
         return FALSE;
      }
   }






   //  glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, textureColorbuffer_);
   //  glTexStorage2DMultisample( GL_TEXTURE_2D_MULTISAMPLE, 16, GL_RGBA8, avm_window_width_, avm_window_height_, GL_TRUE);
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, avm_window_width_, avm_window_height_, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   //  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer_, 0);
   //  // create a renderbuffer object for depth and stencil attachment (we won't be sampling these)
   //  unsigned int rbo;
   //  glGenRenderbuffers(1, &rbo);
   //  glBindRenderbuffer(GL_RENDERBUFFER, rbo);

   //  glRenderbufferStorageMultisample(GL_RENDERBUFFER, 16, GL_RGBA, avm_window_width_, avm_window_height_);
   //  // glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, avm_window_width_, avm_window_height_); // use a single renderbuffer object for both a depth AND stencil buffer.
   //  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo); // now actually attach it

#endif

   glGenTextures ( 1, &userData->shadowMapTextureId );
   glBindTexture ( GL_TEXTURE_2D, userData->shadowMapTextureId );
   glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
   glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
   glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE );
   glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE );
        
   // Setup hardware comparison
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL );
        
   glTexImage2D ( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
                  userData->shadowMapTextureWidth, userData->shadowMapTextureHeight, 
                  0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL );

   glBindTexture ( GL_TEXTURE_2D, 0 );



   
   printf("defaultFramebuffer = %u, userData->test_framebufferId=%u", defaultFramebuffer, userData->test_framebufferId);

   // setup fbo
   glGenFramebuffers ( 1, &userData->shadowMapBufferId );
   glBindFramebuffer ( GL_FRAMEBUFFER, userData->shadowMapBufferId );

   glDrawBuffers ( 1, &none );
   
   glFramebufferTexture2D ( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, userData->shadowMapTextureId, 0 );

   glActiveTexture ( GL_TEXTURE0 );
   glBindTexture ( GL_TEXTURE_2D, userData->shadowMapTextureId );
 
   if ( GL_FRAMEBUFFER_COMPLETE != glCheckFramebufferStatus ( GL_FRAMEBUFFER ) )
   {
      printf("create framebuff failed--2.\n");
      return FALSE;
   }

   glBindFramebuffer ( GL_FRAMEBUFFER, defaultFramebuffer );

   return TRUE;
}

///
// Initialize the shader and program object
//
int Init ( ESContext *esContext )
{
   GLfloat *positions;
   GLuint *indices;

   UserData *userData =(UserData *) esContext->userData;
   const char vShadowMapShaderStr[] =  
      "#version 300 es                                  \n"
      "uniform mat4 u_mvpLightMatrix;                   \n"
      "layout(location = 0) in vec4 a_position;         \n"
      "out vec4 v_color;                                \n"
      "void main()                                      \n"
      "{                                                \n"
      "   gl_Position = u_mvpLightMatrix * a_position;  \n"
      "}                                                \n";
   
   const char fShadowMapShaderStr[] =  
      "#version 300 es                                  \n"
      "precision lowp float;                            \n"
      "void main()                                      \n"
      "{                                                \n"
      "}                                                \n";

    const char vSceneShaderStr[] =  
      "#version 300 es                                   \n"
      "uniform mat4 u_mvpMatrix;                         \n"
      "uniform mat4 u_mvpLightMatrix;                    \n"
      "layout(location = 0) in vec4 a_position;          \n"
      "layout(location = 1) in vec4 a_color;             \n"
      "out vec4 v_color;                                 \n"
      "out vec4 v_shadowCoord;                           \n"
      "void main()                                       \n"
      "{                                                 \n"
      "   v_color = a_color;                             \n"
      "   gl_Position = u_mvpMatrix * a_position;        \n"
      "   v_shadowCoord = u_mvpLightMatrix * a_position; \n"
      "                                                  \n"
      "   // transform from [-1,1] to [0,1];             \n"
      "   v_shadowCoord = v_shadowCoord * 0.5 + 0.5;     \n"
      "}                                                 \n";
   
   const char fSceneShaderStr[] =  
      "#version 300 es                                                \n"
      "precision lowp float;                                          \n"
      "uniform lowp sampler2DShadow s_shadowMap;                      \n"
      "in vec4 v_color;                                               \n"
      "in vec4 v_shadowCoord;                                         \n"
      "layout(location = 0) out vec4 outColor;                        \n"
      "                                                               \n"
      "float lookup ( float x, float y )                              \n"
      "{                                                              \n"
      "   float pixelSizeX = 1.0/2560.0; float pixelSizeY = 1.0/1392.0;                            \n"
      "   vec4 offset = vec4 ( x * pixelSizeX * v_shadowCoord.w,       \n"
      "                        y * pixelSizeY * v_shadowCoord.w,       \n"
      "                        -0.005 * v_shadowCoord.w, 0.0 );       \n"
      "   return textureProj ( s_shadowMap, v_shadowCoord + offset ); \n"
      "}                                                              \n"
      "                                                               \n"
      "void main()                                                    \n"
      "{                                                              \n"
      "   // 3x3 kernel with 4 taps per sample, effectively 6x6 PCF   \n"
      "   float sum = 0.0;                                            \n"
      "   float x, y;                                                 \n"
      "   for ( x = -2.0; x <= 2.0; x += 2.0 )                        \n"
      "      for ( y = -2.0; y <= 2.0; y += 2.0 )                     \n"
      "         sum += lookup ( x, y );                               \n"
      "                                                               \n"
      "   // divide sum by 9.0                                        \n"
      "   sum = sum * 0.11;                                           \n"
      "   outColor = v_color * sum;                                   \n"
      "}                                                              \n";


const char vScreenShaderStr[] =  
"#version 310 es                                   \n"
"layout (location = 0) in vec2 aPos;                                   \n"
"layout (location = 1) in vec2 aTexCoords;                                   \n"
"out vec2 TexCoords;                                   \n"
"void main()                                   \n"
"{                                   \n"
"    TexCoords = aTexCoords;  TexCoords.y = 1.0 - TexCoords.y;                                 \n"
"    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);                                    \n"
"}                                     \n"
;

const char fScreenShaderStr[] =  
"#version 310 es                                                             \n"
"precision mediump float;                                                             \n"
"out vec4 FragColor;                                                             \n"
"in vec2 TexCoords;                                                             \n"
"uniform mediump sampler2DMS screenTexture;      uniform mediump int samples;                       \n"
"void main()                                                             \n"
"{                                                             \n"
"	vec2 tex_coord = TexCoords;                                                             \n"
"   // vec3 col = texture(screenTexture, tex_coord).rgb;                                                             \n"
"  ivec2 size = textureSize(screenTexture);           \n"
"  //int x = TexCoords.x*size.x;                          \n "
"  //int y = (TexCoords.y*size.y);                                                                                 \n"
"  ivec2 tex_coord_multiple = ivec2( TexCoords* vec2(size ));  \n"
"      vec4 ret_color = vec4(0, 0, 0, 0);            \n"
"  //tex_coord_multiple.x *= size.x;    tex_coord_multiple.y *= size.y;                            \n"
"  for(int i=0; i<samples; i++)     \n"
"     ret_color += texelFetch(screenTexture, tex_coord_multiple, i);          \n"
"  ret_color /= float(samples);"
"    FragColor = ret_color;//vec4(col.b,col.g,col.r, 1.0);                                                             \n"
"}                                                             \n" 
;

   userData->msaa_level_ = 4;
   printf("mass level:%d\n", userData->msaa_level_);

   // Load the shaders and get a linked program object
   userData->shadowMapProgramObject = esLoadProgram ( vShadowMapShaderStr, fShadowMapShaderStr );
   userData->sceneProgramObject = esLoadProgram ( vSceneShaderStr, fSceneShaderStr );

   userData->screen_shader_ID_ = esLoadProgram (vScreenShaderStr, fScreenShaderStr);

   // Get the uniform locations
   userData->sceneMvpLoc = glGetUniformLocation ( userData->sceneProgramObject, "u_mvpMatrix" );
   userData->shadowMapMvpLoc = glGetUniformLocation ( userData->shadowMapProgramObject, "u_mvpMatrix" );
   userData->sceneMvpLightLoc = glGetUniformLocation ( userData->sceneProgramObject, "u_mvpLightMatrix" );
   userData->shadowMapMvpLightLoc = glGetUniformLocation ( userData->shadowMapProgramObject, "u_mvpLightMatrix" );

   // Get the sampler location
   userData->shadowMapSamplerLoc = glGetUniformLocation ( userData->sceneProgramObject, "s_shadowMap" );

   // Generate the vertex and index data for the ground
   userData->groundGridSize = 3;
   userData->groundNumIndices = esGenSquareGrid( userData->groundGridSize, &positions, &indices );

   // Index buffer object for the ground model
   glGenBuffers ( 1, &userData->groundIndicesIBO );
   glBindBuffer ( GL_ELEMENT_ARRAY_BUFFER, userData->groundIndicesIBO );
   glBufferData ( GL_ELEMENT_ARRAY_BUFFER, userData->groundNumIndices * sizeof( GLuint ), indices, GL_STATIC_DRAW );
   glBindBuffer ( GL_ELEMENT_ARRAY_BUFFER, 0 );
   free( indices );

   // Position VBO for ground model
   glGenBuffers ( 1, &userData->groundPositionVBO );
   glBindBuffer ( GL_ARRAY_BUFFER, userData->groundPositionVBO );
   glBufferData ( GL_ARRAY_BUFFER, userData->groundGridSize * userData->groundGridSize * sizeof( GLfloat ) * 3, 
                  positions, GL_STATIC_DRAW );
   free( positions );

   // Generate the vertex and index date for the cube model
   userData->cubeNumIndices = esGenCube ( 1.0f, &positions,
                                          NULL, NULL, &indices );

   // Index buffer object for cube model
   glGenBuffers ( 1, &userData->cubeIndicesIBO );
   glBindBuffer ( GL_ELEMENT_ARRAY_BUFFER, userData->cubeIndicesIBO );
   glBufferData ( GL_ELEMENT_ARRAY_BUFFER, sizeof( GLuint ) * userData->cubeNumIndices, indices, GL_STATIC_DRAW );
   glBindBuffer ( GL_ELEMENT_ARRAY_BUFFER, 0 );
   free( indices );

   // Position VBO for cube model
   glGenBuffers ( 1, &userData->cubePositionVBO );
   glBindBuffer ( GL_ARRAY_BUFFER, userData->cubePositionVBO );
   glBufferData ( GL_ARRAY_BUFFER, 24 * sizeof( GLfloat ) * 3, positions, GL_STATIC_DRAW );
   free( positions );

   // setup transformation matrices
   userData->eyePosition[0] = -5.0f;
   userData->eyePosition[1] = 3.0f;
   userData->eyePosition[2] = 5.0f;
   userData->lightPosition[0] = 10.0f;
   userData->lightPosition[1] = 5.0f;
   userData->lightPosition[2] = 2.0f;




       // y-flip texture coordinates
    float quadVertices[] = { // vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 0.0f,
        -1.0f, -1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 1.0f,

        -1.0f,  1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 0.0f
    };

    // screen quad VAO
    GLuint screen_quadVBO;
    glGenVertexArrays(1, &userData->screen_quadVAO_);
    glGenBuffers(1, &screen_quadVBO);
    glBindVertexArray(userData->screen_quadVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, screen_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);



   
   // create depth texture
   if ( !InitShadowMap( esContext ) )
   {
      return FALSE;
   }

   glClearColor ( 1.0f, 1.0f, 1.0f, 0.0f );

   // disable culling
   glDisable ( GL_CULL_FACE );

   // enable depth test
   glEnable ( GL_DEPTH_TEST );
printf("init successful contex\n");
   return TRUE;
}

///
// Draw the model
//
void DrawScene ( ESContext *esContext, 
                 GLint mvpLoc, 
                 GLint mvpLightLoc )
{
   UserData *userData =(UserData *) esContext->userData;
 
   // Draw the ground
   // Load the vertex position
   glBindBuffer ( GL_ARRAY_BUFFER, userData->groundPositionVBO );
   glVertexAttribPointer ( POSITION_LOC, 3, GL_FLOAT, 
                           GL_FALSE, 3 * sizeof(GLfloat), (const void*)NULL );
   glEnableVertexAttribArray ( POSITION_LOC );   

   // Bind the index buffer
   glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, userData->groundIndicesIBO );

   // Load the MVP matrix for the ground model
   glUniformMatrix4fv ( mvpLoc, 1, GL_FALSE, (GLfloat*) &userData->groundMvpMatrix.m[0][0] );
   glUniformMatrix4fv ( mvpLightLoc, 1, GL_FALSE, (GLfloat*) &userData->groundMvpLightMatrix.m[0][0] );

   // Set the ground color to light gray
   glVertexAttrib4f ( COLOR_LOC, 0.9f, 0.9f, 0.9f, 1.0f );

   glDrawElements ( GL_TRIANGLES, userData->groundNumIndices, GL_UNSIGNED_INT, (const void*)NULL );

   // Draw the cube
   // Load the vertex position
   glBindBuffer( GL_ARRAY_BUFFER, userData->cubePositionVBO );
   glVertexAttribPointer ( POSITION_LOC, 3, GL_FLOAT, 
                           GL_FALSE, 3 * sizeof(GLfloat), (const void*)NULL );
   glEnableVertexAttribArray ( POSITION_LOC );   

   // Bind the index buffer
   glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, userData->cubeIndicesIBO );

   // Load the MVP matrix for the cube model
   glUniformMatrix4fv ( mvpLoc, 1, GL_FALSE, (GLfloat*) &userData->cubeMvpMatrix.m[0][0] );
   glUniformMatrix4fv ( mvpLightLoc, 1, GL_FALSE, (GLfloat*) &userData->cubeMvpLightMatrix.m[0][0] );

   // Set the cube color to red
   glVertexAttrib4f ( COLOR_LOC, 1.0f, 0.0f, 0.0f, 1.0f );

   glDrawElements ( GL_TRIANGLES, userData->cubeNumIndices, GL_UNSIGNED_INT, (const void*)NULL );
}

void Draw ( ESContext *esContext )
{

   {
        int samples = 0;
        int sample_buffers = 0;
        glGetIntegerv(GL_SAMPLES, &samples);
        glGetIntegerv(GL_SAMPLE_BUFFERS, &sample_buffers);
      //   printf( "\nRender()  .samples:%d buffers:%d\n" ,samples, sample_buffers );
    }

   UserData *userData =(UserData *) esContext->userData;
   GLint defaultFramebuffer = 0;

   // Initialize matrices
   InitMVP ( esContext );

   glGetIntegerv ( GL_FRAMEBUFFER_BINDING, &defaultFramebuffer );

   // FIRST PASS: Render the scene from light position to generate the shadow map texture
   glBindFramebuffer ( GL_FRAMEBUFFER, userData->shadowMapBufferId );
{
        int samples = 0;
        int sample_buffers = 0;
        glGetIntegerv(GL_SAMPLES, &samples);
        glGetIntegerv(GL_SAMPLE_BUFFERS, &sample_buffers);
      //   printf( "\nRender() shadowMapBufferId .samples:%d buffers:%d\n" ,samples, sample_buffers );
    }
   // Set the viewport
   glViewport ( 0, 0, userData->shadowMapTextureWidth, userData->shadowMapTextureHeight );

   // clear depth buffer
   glClear( GL_DEPTH_BUFFER_BIT );

   // disable color rendering, only write to depth buffer
   glColorMask ( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );

   // reduce shadow rendering artifact
   glEnable ( GL_POLYGON_OFFSET_FILL );
   glPolygonOffset( 5.0f, 100.0f );

   glUseProgram ( userData->shadowMapProgramObject );
   DrawScene ( esContext, userData->shadowMapMvpLoc, userData->shadowMapMvpLightLoc );

   glDisable( GL_POLYGON_OFFSET_FILL );

   // SECOND PASS: Render the scene from eye location using the shadow map texture created in the first pass
   glBindFramebuffer ( GL_FRAMEBUFFER, userData->test_framebufferId );
   glColorMask ( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
{
        int samples = 0;
        int sample_buffers = 0;
        glGetIntegerv(GL_SAMPLES, &samples);
        glGetIntegerv(GL_SAMPLE_BUFFERS, &sample_buffers);
      //   printf( "\ntest_framebufferId() .samples:%d buffers:%d\n" ,samples, sample_buffers );
    }
   // Set the viewport
   glViewport ( 0, 0, esContext->width, esContext->height );
   
   // Clear the color and depth buffers
   glClear ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
   glClearColor ( 1.0f, 1.0f, 1.0f, 0.0f );

   // Use the scene program object
   glUseProgram ( userData->sceneProgramObject );

   // Bind the shadow map texture
   glActiveTexture ( GL_TEXTURE0 );
   glBindTexture ( GL_TEXTURE_2D, userData->shadowMapTextureId );

   // Set the sampler texture unit to 0
   glUniform1i ( userData->shadowMapSamplerLoc, 0 );

   DrawScene ( esContext, userData->sceneMvpLoc, userData->sceneMvpLightLoc );
   InnerCheckGLError(__FILE__, __LINE__);


   int w =  userData->shadowMapTextureWidth;
   int h = userData->shadowMapTextureHeight;
#if 0

   glBindFramebuffer(GL_READ_FRAMEBUFFER, userData->test_framebufferId);
   InnerCheckGLError(__FILE__, __LINE__);
   glBindFramebuffer( GL_DRAW_FRAMEBUFFER, defaultFramebuffer);
   InnerCheckGLError(__FILE__, __LINE__);
   glBlitFramebuffer(0, 0, w, h, 0, 0, w, h,  GL_COLOR_BUFFER_BIT, GL_NEAREST);
   InnerCheckGLError(__FILE__, __LINE__);
   glBindFramebuffer ( GL_FRAMEBUFFER, defaultFramebuffer );
   InnerCheckGLError(__FILE__, __LINE__);

#else
   glBindFramebuffer( GL_DRAW_FRAMEBUFFER, defaultFramebuffer);
   InnerCheckGLError(__FILE__, __LINE__);

   glViewport(0, 0, w, h);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(userData->screen_shader_ID_);
    glUniform1i(glGetUniformLocation(userData->screen_shader_ID_, "screenTexture"), 0);

      glUniform1i(glGetUniformLocation(userData->screen_shader_ID_, "samples"), userData->msaa_level_);

    InnerCheckGLError(__FILE__, __LINE__);
    glBindVertexArray(userData->screen_quadVAO_);
    InnerCheckGLError(__FILE__, __LINE__);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, userData->testTextureId ); // use the color attachment texture as the texture of the quad plane
    InnerCheckGLError(__FILE__, __LINE__);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    InnerCheckGLError(__FILE__, __LINE__);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glBindVertexArray(0);
    InnerCheckGLError(__FILE__, __LINE__);
#endif
}

///
// Cleanup
//
void Shutdown ( ESContext *esContext )
{
   UserData *userData =(UserData *) esContext->userData;

   glDeleteBuffers( 1, &userData->groundPositionVBO );
   glDeleteBuffers( 1, &userData->groundIndicesIBO );

   glDeleteBuffers( 1, &userData->cubePositionVBO );
   glDeleteBuffers( 1, &userData->cubeIndicesIBO );
   
   // Delete shadow map
   glBindFramebuffer ( GL_FRAMEBUFFER, userData->shadowMapBufferId );
   glFramebufferTexture2D ( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0 );
   glBindFramebuffer ( GL_FRAMEBUFFER, 0 );
   glDeleteFramebuffers ( 1, &userData->shadowMapBufferId );
   glDeleteTextures ( 1, &userData->shadowMapTextureId );

   // Delete program object
   glDeleteProgram ( userData->sceneProgramObject );
   glDeleteProgram ( userData->shadowMapProgramObject );
}

int esMain ( ESContext *esContext )
{
   esContext->userData = malloc ( sizeof( UserData ) );

   esCreateWindow ( esContext, "Shadow Rendering", 2560, 1392, ES_WINDOW_RGB | ES_WINDOW_DEPTH |ES_WINDOW_MULTISAMPLE | ES_WINDOW_ALPHA);
   
   if ( !Init ( esContext ) )
   {
      printf("init failed\n");
      return GL_FALSE;
   }
printf("success\n");
   esRegisterShutdownFunc ( esContext, Shutdown );
   esRegisterDrawFunc ( esContext, Draw );
   
   return GL_TRUE;
}

