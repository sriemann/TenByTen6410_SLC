#ifndef _PPGrayFrag_h_
#define _PPGrayFrag_h_


// Header generated from binary by WriteAsBinHeader()..
static const int PPGrayFragLength = 206;
static const unsigned int PPGrayFrag[PPGrayFragLength]={
    0x20205350,    0xFFFF0008,    0x00000048,    0x01020000,    0x00000009,    0x0000001C,    0x00000000,    0x00000000,    0x00000001,    0x00000000,
    0x00000004,    0x00000001,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000027,    0x00000000,    0x00000000,    0x02025400,
    0x03782250,    0x00000000,    0x00000000,    0x0102E407,    0x107821E4,    0x00000000,    0x00000000,    0x01010000,    0x00F820E4,    0x00000000,
    0x03000000,    0x00000002,    0x0D086100,    0x00000000,    0x00000000,    0x06010000,    0x18800200,    0x00000000,    0x04000000,    0x0101E402,
    0x048821E4,    0x00000000,    0x00000000,    0x01010000,    0x00F82000,    0x00000000,    0x00000000,    0x01000000,    0x00FA10E4,    0x00000000,
    0x00000000,    0x00000000,    0x1E000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x3F800000,    0x3F800000,
    0x3F800000,    0x3F800000,    0x3F800000,    0x00000000,    0x00000000,    0x00000000,    0x3F000000,    0x00000000,    0x00000000,    0x00000000,
    0x3EA8F5C3,    0x3EA8F5C3,    0x3EA8F5C3,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,
    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,
    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,
    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,
    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,
    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,
    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,
    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,
    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,
    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x00000000,    0x0000001E,    0x00000008,    0x00000003,    0x00000000,
    0x00000000,    0x00000000,    0x00000007,    0x00000003,    0x00020001,    0x0000006C,    0x0000000C,    0x00000004,    0x00000000,    0x00020001,
    0x00000020,    0x00000011,    0x00000005,    0x00000000,    0x00020001,    0x00000064,    0x00000017,    0x00000006,    0x00000000,    0x00020001,
    0x00000068,    0x00000008,    0x00000003,    0x0000000F,    0x00030005,    0x00000000,    0x44786554,    0x00306D69,    0x00786574,    0x656D6974,
    0x64697700,    0x68006874,    0x68676965,    0x65740074,    0x6F6F4378,    0x00006472,};

//checksum generated by simpleCheckSum()
static const unsigned int PPGrayFragCheckSum = 190;

static const char* PPGrayFragText = 
    "\n"
    "\n"
    "#ifdef GL_ES\n"
    "precision highp float;\n"
    "#endif \n"
    "\n"
    "uniform sampler2D tex;\n"
    "uniform float time;\n"
    "\n"
    "uniform float width;\n"
    "uniform float height;\n"
    "\n"
    "const vec4 offsetcolor=vec4(0.1,0.1,0.1,0.1);\n"
    "const float threshold=0.2;\n"
    "\n"
    "varying vec2 texCoord;\n"
    "\n"
    "\n"
    "const vec4 grayscalewt = vec4(0.33,0.33,0.33,0.0);\n"
    "\n"
    "vec4 wobbleGray();\n"
    "vec4 costlyBlur();\n"
    "vec4 edgeDetect();\n"
    "vec4 waves();\n"
    "vec4 ripples();\n"
    "vec4 tiles();\n"
    "\n"
    "void main()\n"
    "{\n"
    "        vec2 modTexCoord = texCoord;//+vec2(0.03*sin(30.0*texCoord.y+10.0*time),0.0);\n"
    "        \n"
    "        vec4 color = texture2D(tex, modTexCoord);\n"
    "        \n"
    "        if(modTexCoord.x > 0.5)\n"
    "        {\n"
    "                color = vec4(dot(color, grayscalewt));\n"
    "        }\n"
    "        \n"
    "        gl_FragColor =  color;\n"
    "        //gl_FragColor = wobbleGray();\n"
    "        //gl_FragColor = edgeDetect();\n"
    "        //gl_FragColor = waves();\n"
    "        //gl_FragColor = ripples();\n"
    "        //gl_FragColor = tiles();\n"
    "}\n"
    "\n"
    "vec4 wobbleGray()\n"
    "{\n"
    "        vec2 modTexCoord = texCoord+vec2(0.03*sin(30.0*texCoord.y+10.0*time),0.0);\n"
    "        vec4 color = texture2D(tex, modTexCoord);\n"
    "        \n"
    "        if(modTexCoord.x > 0.5)\n"
    "        {\n"
    "                color = vec4(dot(color, grayscalewt));\n"
    "        }\n"
    "        \n"
    "        return color;\n"
    "        \n"
    "}\n"
    "\n"
    "vec4 waves()\n"
    "{\n"
    "        vec2 modTexCoord = texCoord+0.03*vec2(sin(10.0*texCoord.x+20.0*time), cos(10.0*texCoord.y+20.0*time));\n"
    "        \n"
    "        return texture2D(tex, modTexCoord);\n"
    "}\n"
    "\n"
    "vec4 ripples()\n"
    "{\n"
    "        vec2 radialVec = (texCoord-vec2(0.5,0.5));\n"
    "        float radialDist = length(radialVec);\n"
    "        radialVec = normalize(radialVec);        \n"
    "        vec2 modTexCoord = texCoord+0.08*sin(20.0*radialDist+10.0*time)*radialVec;\n"
    "        \n"
    "        return texture2D(tex, modTexCoord);     \n"
    "}\n"
    "\n"
    "vec4 costlyBlur()\n"
    "{\n"
    "        return vec4(1.0,1.0,1.0,1.0);\n"
    "}\n"
    "\n"
    "vec4 tiles()\n"
    "//void main()\n"
    "{\n"
    "\n"
    "    float numtiles = 30.0+20.0*sin(time);\n"
    "    float P0 = texCoord.x;\n"
    "    float P1 = texCoord.y;\n"
    "    float size = 1.0 / numtiles;\n"
    "    float P0base = P0 - mod(P0, size);\n"
    "    float P1base = P1 - mod(P1, size);\n"
    "    float P0center = P0base + size / 2.0;\n"
    "    float P1center = P1base + size / 2.0;\n"
    "\n"
    "    float ss = (P0 - P0base) / size;\n"
    "    float tt = (P1 - P1base) / size;\n"
    "\n"
    "    float thresholdA = threshold;\n"
    "    float thresholdB = 1.0 - threshold;\n"
    "\n"
    "    vec4 c1, c2, cTop, cBottom;\n"
    "    c1 = ss > tt ? offsetcolor : vec4 (0.0,0.0,0.0,1.0);\n"
    "    c2 = ss > thresholdB ? c1 : vec4 (0.0,0.0,0.0,1.0);\n"
    "    c2 = tt < thresholdA ? c1 : c2;\n"
    "    cBottom = c2;\n"
    "\n"
    "    c1 = ss < tt ? offsetcolor : vec4 (0.0,0.0,0.0,1.0);\n"
    "    c2 = ss < thresholdA ? c1 : vec4 (0.0,0.0,0.0,1.0);\n"
    "    c2 = tt > thresholdB ? c1 : c2;\n"
    "    cTop = c2;\n"
    "\n"
    "    vec4 newP = vec4 (P0center,P1center,0.0,1.0);\n"
    "    vec4 tilecolor = texture2D(tex, newP.xy);\n"
    "\n"
    "    return tilecolor + cTop - cBottom;\n"
    "}\n"
    "\n"
    "vec4 edgeDetect()\n"
    "{\n"
    "        vec2 stepScale = vec2(1.0/width, 1.0/height);\n"
    "        float offsetx = 0.5/width;\n"
    "        float offsety = 0.5/height;\n"
    "        \n"
    "        vec4 gxColor = vec4(0.0,0.0,0.0,0.0);\n"
    "        vec4 gyColor = vec4(0.0,0.0,0.0,0.0);\n"
    "        \n"
    "        vec4 color;\n"
    "        \n"
    "        //Top row\n"
    "    vec4 c  = texture2D(tex, texCoord);         \n"
    "\n"
    "    vec4 other = texture2D(tex, texCoord + vec2(-offsetx, -offsety));   \n"
    "    other += texture2D(tex, texCoord + vec2(-offsetx,       0.0));   \n"
    "    other += texture2D(tex, texCoord + vec2(-offsetx,  offsety));   \n"
    "    other += texture2D(tex, texCoord + vec2(      0.0,  offsety));   \n"
    "    other += texture2D(tex, texCoord + vec2( offsetx,  offsety));   \n"
    "    other += texture2D(tex, texCoord + vec2( offsetx,       0.0));   \n"
    "    other += texture2D(tex, texCoord + vec2( offsetx,  offsety));   \n"
    "    other += texture2D(tex, texCoord + vec2(      0.0, -offsety));   \n"
    "\n"
    "        \n"
    "    return vec4(length(8.0*c -other));\n"
    "}\n"
    "";

#ifdef GL_HELPERS_INCLUDED
//glHelpers.h must be included BEFORE any of the shader header files. Also make sure you have the latest version of glHelpers.h
static ghShader PPGrayFragShader(PPGrayFragText, PPGrayFrag, PPGrayFragLength, PPGrayFragCheckSum);


#endif


#endif //_PPGrayFrag_h_
