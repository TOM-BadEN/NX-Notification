#pragma once

#include <switch.h>

// STB TrueType 实现
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// 字体管理器（单例）
// 负责加载和管理 Switch 系统共享字体，全局只初始化一次
class FontManager {
public:
    // 字形位图结构
    struct GlyphBitmap {
        u8* data;           // 位图数据
        int width;          // 宽度
        int height;         // 高度
        int xoffset;        // X 偏移
        int yoffset;        // Y 偏移
        int advance;        // 字符前进距离
    };
    
    // 获取单例实例（第一次调用时自动加载字体）
    static FontManager& Instance() {
        static FontManager instance;  
        return instance;
    }
    
    // 获取标准字体（英文、数字、基本符号）
    stbtt_fontinfo* GetStdFont() { return &m_FontStd; }
    
    // 获取本地化字体（中文/韩文等，根据系统语言）
    stbtt_fontinfo* GetLocalFont() { return &m_FontLocal; }
    
    // 获取扩展字体（任天堂图标和特殊符号）
    stbtt_fontinfo* GetExtFont() { return &m_FontExt; }
    
    // 计算缩放比例，使 fontSize 代表实际可见字符高度（公开给 GraphicsRenderer 使用）
    float CalculateScaleForVisibleHeight(stbtt_fontinfo* font, float fontSize) {
        // 获取大写字母 'H' 的边界框
        int x0, y0, x1, y1;
        stbtt_GetCodepointBox(font, 'H', &x0, &y0, &x1, &y1);
        
        float capHeight = y1 - y0;  // 字体单位下的大写字母高度
        
        // 如果获取失败，使用字体度量估算
        if (capHeight <= 0) {
            int ascent, descent, lineGap;
            stbtt_GetFontVMetrics(font, &ascent, &descent, &lineGap);
            capHeight = ascent * 0.7f;  // 大写字母约占 ascent 的 70%
        }
        
        // 返回缩放比例，使 capHeight 缩放后等于 fontSize
        return fontSize / capHeight;
    }
    
    // 根据码点渲染字形位图（自动选择字体）
    GlyphBitmap RenderGlyph(u32 codepoint, float fontSize) {
        GlyphBitmap glyph = {nullptr, 0, 0, 0, 0, 0};
        
        // 选择字体
        stbtt_fontinfo* font = PickFontForCodepoint(codepoint);
        if (!font) return glyph;
        
        // 计算缩放比例（基于大写字母高度，让 fontSize 代表实际可见高度）
        float scale = CalculateScaleForVisibleHeight(font, fontSize);
        
        // 获取字形位图
        glyph.data = stbtt_GetCodepointBitmap(
            font, 
            scale, scale,
            (int)codepoint,
            &glyph.width, &glyph.height,
            &glyph.xoffset, &glyph.yoffset
        );
        
        // 获取字符前进距离
        int leftSideBearing;
        stbtt_GetCodepointHMetrics(font, (int)codepoint, &glyph.advance, &leftSideBearing);
        glyph.advance = (int)(glyph.advance * scale);
        
        return glyph;
    }
    
    // 释放字形位图
    void FreeGlyph(GlyphBitmap& glyph) {
        if (glyph.data) {
            stbtt_FreeBitmap(glyph.data, nullptr);
            glyph.data = nullptr;
        }
    }
    
private:
    // 构造函数：加载所有字体
    FontManager() : m_HasLocalFont(false), m_HasExtFont(false) {
        PlFontData font;
        
        // 1. 加载标准字体（英文、数字、基本符号）
        if (R_SUCCEEDED(plGetSharedFontByType(&font, PlSharedFontType_Standard))) {
            stbtt_InitFont(&m_FontStd, (u8*)font.address, stbtt_GetFontOffsetForIndex((u8*)font.address, 0));
        }
        
        // 2. 加载任天堂扩展字体（图标和特殊符号）
        if (R_SUCCEEDED(plGetSharedFontByType(&font, PlSharedFontType_NintendoExt))) {
            stbtt_InitFont(&m_FontExt, (u8*)font.address, stbtt_GetFontOffsetForIndex((u8*)font.address, 0));
            m_HasExtFont = true;
        }
        
        // 3. 根据系统语言加载本地化字体
        u64 langCode = 0;
        
        if (R_SUCCEEDED(setGetSystemLanguage(&langCode))) {
            PlSharedFontType type = PlSharedFontType_Standard;
            
            // LanguageCode 是一个字符串（以 u64 存储，小端序）
            // 根据语言码选择对应的字体类型
            switch (langCode) {
                case 0x736E61482D687AULL:  // "zh-Hans" 简体中文
                case 0x4E432D687AULL:       // "zh-CN" 简体中文（旧格式）
                    type = PlSharedFontType_ChineseSimplified;
                    break;
                case 0x746E61482D687AULL:  // "zh-Hant" 繁体中文
                case 0x57542D687AULL:       // "zh-TW" 繁体中文（旧格式）
                    type = PlSharedFontType_ChineseTraditional;
                    break;
                case 0x6F6BULL:             // "ko" 韩文
                    type = PlSharedFontType_KO;
                    break;
                default:
                    type = PlSharedFontType_Standard;
                    break;
            }
            
            // 加载对应的本地化字体
            if (type != PlSharedFontType_Standard && R_SUCCEEDED(plGetSharedFontByType(&font, type))) {
                stbtt_InitFont(&m_FontLocal, (u8*)font.address, stbtt_GetFontOffsetForIndex((u8*)font.address, 0));
                m_HasLocalFont = true;
            }
        }
    }
    
    ~FontManager() = default;
    
    // 禁止拷贝和赋值（单例模式）
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;
    
    // 根据码点选择字体（优先级：本地化 > 扩展 > 标准）
    stbtt_fontinfo* PickFontForCodepoint(u32 codepoint) {
        // 1. 优先检查本地化字体（中文、韩文等）
        if (m_HasLocalFont && stbtt_FindGlyphIndex(&m_FontLocal, (int)codepoint) != 0) {
            return &m_FontLocal;
        }
        
        // 2. 检查扩展字体（图标和特殊符号）
        if (m_HasExtFont && stbtt_FindGlyphIndex(&m_FontExt, (int)codepoint) != 0) {
            return &m_FontExt;
        }
        
        // 3. 回退到标准字体
        return &m_FontStd;
    }
    
    // 字体对象（只保存指向系统共享内存的指针，不占用大量内存）
    stbtt_fontinfo m_FontStd;      // 标准字体（英文、数字、基本符号）
    stbtt_fontinfo m_FontLocal;    // 本地化字体（中文、韩文等）
    stbtt_fontinfo m_FontExt;      // 扩展字体（任天堂图标和特殊符号）
    bool m_HasLocalFont;           // 本地化字体是否已加载
    bool m_HasExtFont;             // 扩展字体是否已加载
};

