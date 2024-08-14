#include "TextureLoader.h"
#include "Utils/assert.h"
//#define STB_IMAGE_IMPLEMENTATION      //< Implementations are already defined in ImFileDialog
#include "stb/stb_image.h"
#include "ddspp/ddspp.h"
#include <fstream>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize2.h"

static inline RHITextureType GetTextureType(ddspp::TextureType type, bool array)
{
    switch (type)
    {
        case ddspp::Texture2D:
            return array ? RHITextureType::Texture2DArray : RHITextureType::Texture2D;
        case ddspp::Texture3D:
            return array ? RHITextureType::TextureCubeArray : RHITextureType::Texture3D;
        case ddspp::Texture1D:
        default:
            MY_ASSERT(false);
            return RHITextureType::Texture2D;
    }
}

static inline RHIFormat GetTextureFormat(ddspp::DXGIFormat format, bool srgb)
{
  switch(format)
  {
  case ddspp::UNKNOWN:
      return RHIFormat::Unknown;
  case ddspp::R32G32B32A32_FLOAT:
      return RHIFormat::RGBA32F;
  case ddspp::R32G32B32A32_UINT:
      return RHIFormat::RGBA32UI;
  case ddspp::R32G32B32A32_SINT:
      return RHIFormat::RGBA32SI;
  case ddspp::R16G16B16A16_FLOAT:
      return RHIFormat::RGBA16F;
  case ddspp::R16G16B16A16_UINT:
      return RHIFormat::RGBA16UI;
  case ddspp::R16G16B16A16_SINT:
      return RHIFormat::RGBA16SI;
  case ddspp::R16G16B16A16_UNORM:
      return RHIFormat::RGBA16UNORM;
  case ddspp::R16G16B16A16_SNORM:
      return RHIFormat::RGBA16SNORM;
  case ddspp::R8G8B8A8_UINT:
      return RHIFormat::RGBA8UI;
  case ddspp::R8G8B8A8_SINT:
      return RHIFormat::RGBA8SI;
  case ddspp::R8G8B8A8_UNORM:
      return srgb ? RHIFormat::RGBA8SRGB : RHIFormat::RGBA8UNORM;
  case ddspp::R8G8B8A8_SNORM:
      return RHIFormat::RGBA8SNORM;
  case ddspp::R8G8B8A8_UNORM_SRGB:
      return RHIFormat::RGBA8SRGB;
  case ddspp::B8G8R8A8_UNORM:
      return srgb ? RHIFormat::BGRA8SRGB : RHIFormat::BGRA8UNORM;
  case ddspp::B8G8R8A8_UNORM_SRGB:
      return RHIFormat::BGRA8SRGB;
  case ddspp::R9G9B9E5_SHAREDEXP:
      return RHIFormat::RGB9E5;
  case ddspp::R32G32_FLOAT:
      return RHIFormat::RG32F;
  case ddspp::R32G32_UINT:
      return RHIFormat::RG32UI;
  case ddspp::R32G32_SINT:
      return RHIFormat::RG32SI;
  case ddspp::R16G16_FLOAT:
      return RHIFormat::RG16F;
  case ddspp::R16G16_UINT:
      return RHIFormat::RG16UI;
  case ddspp::R16G16_SINT:
      return RHIFormat::RG16SI;
  case ddspp::R16G16_UNORM:
      return RHIFormat::RG16UNORM;
  case ddspp::R16G16_SNORM:
      return RHIFormat::RG16SNORM;
  case ddspp::R8G8_UINT:
      return RHIFormat::RG8UI;
  case ddspp::R8G8_SINT:
      return RHIFormat::RG8SI;
  case ddspp::R8G8_UNORM:
      return RHIFormat::RG8UNORM;
  case ddspp::R8G8_SNORM:
      return RHIFormat::RG8SNORM;
  case ddspp::R32_FLOAT:
      return RHIFormat::R32F;
  case ddspp::R32_UINT:
      return RHIFormat::R32UI;
  case ddspp::R32_SINT:
      return RHIFormat::R32SI;
  case ddspp::R16_FLOAT:
      return RHIFormat::R16F;
  case ddspp::R16_UINT:
      return RHIFormat::R16UI;
  case ddspp::R16_SINT:
      return RHIFormat::R16SI;
  case ddspp::R16_UNORM:
      return RHIFormat::R16UNORM;
  case ddspp::R16_SNORM:
      return RHIFormat::R16SNORM;
  case ddspp::R8_UINT:
      return RHIFormat::R8UI;
  case ddspp::R8_SINT:
      return RHIFormat::R8SI;
  case ddspp::R8_UNORM:
      return RHIFormat::R8UNORM;
  case ddspp::R8_SNORM:
      return RHIFormat::R8SNORM;
  case ddspp::BC1_UNORM:
      return srgb ? RHIFormat::BC1SRGB : RHIFormat::BC1UNORM;
  case ddspp::BC1_UNORM_SRGB:
      return RHIFormat::BC1SRGB;
  case ddspp::BC2_UNORM:
      return srgb ? RHIFormat::BC2SRGB : RHIFormat::BC2UNORM;
  case ddspp::BC2_UNORM_SRGB:
      return RHIFormat::BC2SRGB;
  case ddspp::BC3_UNORM:
      return srgb ? RHIFormat::BC3SRGB : RHIFormat::BC3UNORM;
  case ddspp::BC3_UNORM_SRGB:
      return RHIFormat::BC3SRGB;
  case ddspp::BC4_UNORM:
      return RHIFormat::BC4UNORM;
  case ddspp::BC4_SNORM:
      return RHIFormat::BC4SNORM;
  case ddspp::BC5_UNORM:
      return RHIFormat::BC5UNORM;
  case ddspp::BC5_SNORM:
      return RHIFormat::BC5SNORM;
  case ddspp::BC6H_UF16:
      return RHIFormat::BC6U16F;
  case ddspp::BC6H_SF16:
      return RHIFormat::BC6S16F;
  case ddspp::BC7_UNORM:
      return srgb ? RHIFormat::BC7SRGB : RHIFormat::BC7UNORM;
  case ddspp::BC7_UNORM_SRGB:
      return RHIFormat::BC7SRGB;
  default:
      MY_ASSERT(false);
      return RHIFormat::Unknown;
  }
}

TextureLoader::TextureLoader()
{

}

TextureLoader::~TextureLoader()
{
    if (m_pDecompressedData)
    {
        stbi_image_free(m_pDecompressedData);
    }
}

bool TextureLoader::Load(const eastl::string& file, bool srgb)
{
    std::ifstream is;
    is.open(file.c_str(), std::ios::binary);
    if (is.fail())
    {
        return false;
    }

    is.seekg(0, std::ios::end);
    uint32_t length = (uint32_t)is.tellg();
    is.seekg(0, std::ios::beg);

    m_fileData.resize(length);
    char* pBuffer = (char*) m_fileData.data();

    is.read(pBuffer, length);
    is.close();

    if (file.find(".dds") != eastl::string::npos)
    {
        return LoadDDS(srgb);
    }
    else
    {
        return LoadSTB(srgb);
    }
}

bool TextureLoader::LoadDDS(bool srgb)
{
    uint8_t* pData = m_fileData.data();

    ddspp::Descriptor desc;
    ddspp::Result result = ddspp::decode_header((unsigned char*) pData, desc);
    if (result != ddspp::Success)
    {
        return false;
    }

    m_width = desc.width;
    m_height = desc.height;
    m_depth = desc.depth;
    m_levels = desc.numMips;
    m_arraySize = desc.arraySize;
    m_type = GetTextureType(desc.type, desc.arraySize > 1);
    m_format = GetTextureFormat(desc.format, srgb);

    m_pTextureData = pData + desc.headerSize;
    m_textureSize = (uint32_t) m_fileData.size() - desc.headerSize;

    return true;
}

bool TextureLoader::LoadSTB(bool srgb)
{
    int x, y, comp;
    stbi_info_from_memory((stbi_uc*)m_fileData.data(), (int)m_fileData.size(), &x, &y, &comp);
    
    bool isHDR = stbi_is_hdr_from_memory((stbi_uc*)m_fileData.data(), (int)m_fileData.size());
    bool is16Bit = stbi_is_16_bit_from_memory((stbi_uc*)m_fileData.data(), (int)m_fileData.size());

    if (isHDR)
    {
        m_pDecompressedData = stbi_loadf_from_memory((stbi_uc*)m_fileData.data(), (int)m_fileData.size(), &x, &y, &comp, 0);
        switch (comp)
        {
            case 1:
                m_format = RHIFormat::R32F;
                break;
            case 2:
                m_format = RHIFormat::RG32F;
                break;
            case 3:
                m_format = RHIFormat::RGB32F;
                break;
            case 4:
                m_format = RHIFormat::RGBA32F;
                break;
            default:
                MY_ASSERT(false);
                break;
        }
    }
    else if (is16Bit)
    {
        int desiredChannels = comp == 3 ? 4 : 0;
        m_pDecompressedData = stbi_load_16_from_memory((stbi_uc*)m_fileData.data(), (int)m_fileData.size(), &x, &y, &comp, desiredChannels);
        switch (comp)
        {
            case 1:
                m_format = RHIFormat::R16UNORM;
                break;
            case 2:
                m_format = RHIFormat::RG16UNORM;
                break;
            case 3:
            case 4:
                m_format = RHIFormat::RGBA16UNORM;
                break;
            default:
                MY_ASSERT(false);
                break;
        }
    }
    else
    {
        int desiredChannels = comp == 3 ? 4 : 0;
        m_pDecompressedData = stbi_load_from_memory((stbi_uc*)m_fileData.data(), (int)m_fileData.size(), &x, &y, &comp, desiredChannels);
        switch (comp)
        {
            case 1:
                m_format = RHIFormat::R8UNORM;
                break;
            case 2:
                m_format = RHIFormat::RG8UNORM;
                break;
            case 3:
            case 4:
                m_format = srgb ? RHIFormat::RGBA8SRGB : RHIFormat::RGBA8UNORM;
                break;
            default:
                MY_ASSERT(false);
                break;
        }
    }

    if (m_pDecompressedData == nullptr)
    {
        return false;
    }

    m_width = x;
    m_height = y;
    m_textureSize = GetFormatRowPitch(m_format, x) * y;
    
    return true;
}

bool TextureLoader::Resize(uint32_t width, uint32_t height)
{
    if (m_pDecompressedData == nullptr)
    {
        return false;
    }

    unsigned char* outputData = (unsigned char*) malloc(stbir_pixel_layout::STBIR_RGBA * width * height);
    unsigned char* result = stbir_resize_uint8_linear((const unsigned char*) m_pDecompressedData, m_width, m_height, 0, outputData, width, height, 0, stbir_pixel_layout::STBIR_RGBA); 
    if (result == nullptr || outputData == nullptr) //< result should equal outputData
    {
        return false;
    }
    
    stbi_image_free(m_pDecompressedData);

    m_width = width;
    m_height = height;
    m_pDecompressedData = outputData;
    return true;
}