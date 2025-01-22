#pragma once

#include "Common.hlsli"

namespace debug
{
    struct LineVertex
    {
        float3 m_position;
        uint m_color;
    };
    
    void DrawLine(float3 start, float3 end, float3 color)
    {
        RWByteAddressBuffer argumentsBuffer = ResourceDescriptorHeap[SceneCB.m_debugLineDrawCommandUAV];
        RWStructuredBuffer<LineVertex> vertexBuffer = ResourceDescriptorHeap[SceneCB.m_debugLineVertexBufferUAV];
        
        uint vertexCount;
        argumentsBuffer.InterlockedAdd(0, 2, vertexCount);  // Increment vertex count by 2
        
        LineVertex p;
        p.m_color = PackRGBA8Unorm(float4(color, 1.0));
        
        p.m_position = start;
        vertexBuffer[vertexCount] = p;
        p.m_position = end;
        vertexBuffer[vertexCount + 1] = p;
    }
    
    void DrawTriangle(float3 p0, float3 p1, float3 p2, float3 color)
    {
        DrawLine(p0, p1, color);
        DrawLine(p1, p2, color);
        DrawLine(p2, p0, color);
    }
    
    void DrawBox(float3 min, float3 max, float3 color)
    {
        float3 p0 = float3(min.x, min.y, min.z);
        float3 p1 = float3(max.x, min.y, min.z);
        float3 p2 = float3(max.x, max.y, min.z);
        float3 p3 = float3(min.x, max.y, min.z);
        float3 p4 = float3(min.x, min.y, max.z);
        float3 p5 = float3(max.x, min.y, max.z);
        float3 p6 = float3(max.x, max.y, max.z);
        float3 p7 = float3(min.x, max.y, max.z);
        
        DrawLine(p0, p1, color);
        DrawLine(p1, p2, color);
        DrawLine(p2, p3, color);
        DrawLine(p3, p4, color);
        
        DrawLine(p4, p5, color);
        DrawLine(p5, p6, color);
        DrawLine(p6, p7, color);
        DrawLine(p7, p4, color);
        
        DrawLine(p0, p4, color);
        DrawLine(p1, p5, color);
        DrawLine(p2, p6, color);
        DrawLine(p3, p7, color);
    }
    
    static const uint DEBUG_SPHERE_M = 10; //< Latitude (Horizontal)
    static const uint DEBUG_SPHERE_N = 20; //< Longitude (Vertical)
    
    float3 SpherePosition(uint m, uint n, float3 center, float radius)
    {
        float x = sin(M_PI * m / DEBUG_SPHERE_M) * cos(2 * M_PI * n / DEBUG_SPHERE_N);
        float z = sin(M_PI * m / DEBUG_SPHERE_M) * sin(2 * M_PI * n / DEBUG_SPHERE_N);
        float y = cos(M_PI * m / DEBUG_SPHERE_M);
        return center + float3(x, y, z) * radius;
    }
    
    void DrawSphere(float3 center, float radius, float3 color)
    {
        for(uint m = 0; m < DEBUG_SPHERE_M; ++ m)
        {
            for(uint n = 0; n < DEBUG_SPHERE_N; ++ n)
            {
                float3 p0 = SpherePosition(m, n, center, radius);
                float3 p1 = SpherePosition(min(m + 1, DEBUG_SPHERE_M), n, center, radius);
                float3 p2 = SpherePosition(m, min(n + 1, DEBUG_SPHERE_N), center, radius);
                
                DrawTriangle(p0, p1, p2, color);
            }
        }
    }
    
    struct Text
    {
        float2 m_screenPosition;
        uint m_text;
        uint m_color;
    };
    
    // packed version if sbtt_bakedchar
    struct BakedChar
    {
        uint x0 : 8; // coordinates of bbix in bitmap
        uint y0 : 8;
        uint x1 : 8;
        uint y1 : 8;
        
        float xoff;
        float yoff;
        float xadvance;
    };
    
    void PrintChar(inout float2 screenPos, float3 color, uint text)
    {
        RWByteAddressBuffer counterBuffer = ResourceDescriptorHeap[SceneCB.m_debugTextCounterBufferUAV];
        RWStructuredBuffer<Text> textBuffer = ResourceDescriptorHeap[SceneCB.m_debugTextBufferUAV];
        StructuredBuffer<BakedChar> bakedCharBuffer = ResourceDescriptorHeap[SceneCB.m_debugFontCharBufferSRV];
        
        uint textCount;
        counterBuffer.InterlockedAdd(0, 1, textCount);
        
        textBuffer[textCount].m_screenPosition = screenPos;
        textBuffer[textCount].m_text = text;
        textBuffer[textCount].m_color = PackRGBA8Unorm(float4(color, 1.0));
        
        screenPos.x += bakedCharBuffer[text].xadvance;
    }
    
    void PrintString(inout float2 screenPos, float3 color, uint c1, uint c2)
    {
        PrintChar(screenPos, color, c1);
        PrintChar(screenPos, color, c2);
    }
    
    void PrintString(inout float2 screenPos, float3 color, uint c1, uint c2, uint c3)
    {
        PrintString(screenPos, color, c1, c2);
        PrintChar(screenPos, color, c3);
    }
    
    void PrintString(inout float2 screenPos, float3 color, uint c1, uint c2, uint c3, uint c4)
    {
        PrintString(screenPos, color, c1, c2, c3);
        PrintChar(screenPos, color, c4);
    }
    
    void PrintString(inout float2 screenPos, float3 color, uint c1, uint c2, uint c3, uint c4, uint c5)
    {
        PrintString(screenPos, color, c1, c2, c3, c4);
        PrintChar(screenPos, color, c5);
    }
    
    void PrintString(inout float2 screenPos, float3 color, uint c1, uint c2, uint c3, uint c4, uint c5, uint c6)
    {
        PrintString(screenPos, color, c1, c2, c3, c4, c5);
        PrintChar(screenPos, color, c6);
    }
    
    void PrintString(inout float2 screenPos, float3 color, uint c1, uint c2, uint c3, uint c4, uint c5, uint c6, uint c7)
    {
        PrintString(screenPos, color, c1, c2, c3, c4, c5, c6);
        PrintChar(screenPos, color, c7);
    }
    
    void PrintString(inout float2 screenPos, float3 color, uint c1, uint c2, uint c3, uint c4, uint c5, uint c6, uint c7, uint c8)
    {
        PrintString(screenPos, color, c1, c2, c3, c4, c5, c6, c7);
        PrintChar(screenPos, color, c8);
    }
    
    void PrintString(inout float2 screenPos, float3 color, uint c1, uint c2, uint c3, uint c4, uint c5, uint c6, uint c7, uint c8, uint c9)
    {
        PrintString(screenPos, color, c1, c2, c3, c4, c5, c6, c7, c8);
        PrintChar(screenPos, color, c9);
    }
    
    void PrintString(inout float2 screenPos, float3 color, uint c1, uint c2, uint c3, uint c4, uint c5, uint c6, uint c7, uint c8, uint c9, uint c10)
    {
        PrintString(screenPos, color, c1, c2, c3, c4, c5, c6, c7, c8, c9);
        PrintChar(screenPos, color, c10);
    }
    
    uint GetDigitChar(uint number) // 0 - 9
    {
        return clamp(number, 0, 9) + '0';    
    }
    
    void PrintInt(inout float2 screenPos, float3 color, int number)
    {
        if(number < 0)
        {
            PrintChar(screenPos, color, '-');
            number = -number;
        }
        
        uint length = number == 0 ? 1 : log10(number) + 1;
        uint divisor = round(pow(10, length - 1));
        
        for(uint i = 0; i < length; ++ i)
        {
            uint digit = number / divisor;
            PrintChar(screenPos, color, GetDigitChar(digit));
            
            number = number - digit * divisor;
            divisor /= 10;
        }
    }
    
    void PrintFloat(inout float2 screenPos, float3 color, float number)
    {
        if(isinf(number))
        {
            PrintString(screenPos, color, 'I', 'N', 'F');   
        }
        else if(isnan(number))
        {
            PrintString(screenPos, color, 'N', 'A', 'N');
        }
        else
        {
            if(number < 0)
            {
                PrintChar(screenPos, color, '-');
                number = -number;
            }
            
            PrintInt(screenPos, color, (int) number);
            PrintChar(screenPos, color, '.');
            
            float fracValue = frac(number);
            float presicion = 100000;
            while(fracValue < 0.1 && fracValue > 0.0)
            {
                PrintChar(screenPos, color, '0');
                fracValue *= 10;
                presicion *= 0.1;
            }
            
            fracValue *= presicion;
            PrintInt(screenPos, color, fracValue);
        }
    }
    
    void PrintInt(inout float2 screenPos, float3 color, int2 value)
    {
        PrintInt(screenPos, color, value.x);
        PrintChar(screenPos, color, ',');
        PrintInt(screenPos, color, value.y);
    }
    
    void PrintInt(inout float2 screenPos, float3 color, int3 value)
    {
        PrintInt(screenPos, color, value.xy);
        PrintChar(screenPos, color, ',');
        PrintInt(screenPos, color, value.z);
    }
    
    void PrintInt(inout float2 screenPos, float3 color, int4 value)
    {
        PrintInt(screenPos, color, value.xyz);
        PrintChar(screenPos, color, ',');
        PrintInt(screenPos, color, value.w);
    }
    
    void PrintFloat(inout float2 screenPos, float3 color, float2 value)
    {
        PrintFloat(screenPos, color, value.x);
        PrintChar(screenPos, color, ',');
        PrintFloat(screenPos, color, value.y);
    }
    
    void PrintFloat(inout float2 screenPos, float3 color, float3 value)
    {
        PrintFloat(screenPos, color, value.xy);
        PrintChar(screenPos, color, ',');
        PrintFloat(screenPos, color, value.z);
    }
    
    void PrintFloat(inout float2 screenPos, float3 color, float4 value)
    {
        PrintFloat(screenPos, color, value.xyz);
        PrintChar(screenPos, color, ',');
        PrintFloat(screenPos, color, value.w);
    }
}