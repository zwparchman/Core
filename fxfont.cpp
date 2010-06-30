/*
    Copyright (c) 2009 Andrew Caudwell (acaudwell@gmail.com)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. The name of the author may not be used to endorse or promote products
       derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    Portions of this code are derivived from FTGL (FTTextureFont.cpp)

    FTGL - OpenGL font library

    Copyright (c) 2001-2004 Henry Maddocks <ftgl@opengl.geek.nz>
    Copyright (c) 2008 Sam Hocevar <sam@zoy.org>

    Permission is hereby granted, free of charge, to any person obtaining
    a copy of this software and associated documentation files (the
    "Software"), to deal in the Software without restriction, including
    without limitation the rights to use, copy, modify, merge, publish,
    distribute, sublicense, and/or sell copies of the Software, and to
    permit persons to whom the Software is furnished to do so, subject to
    the following conditions:
 
    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.
 
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "fxfont.h"

FXFontManager fontmanager;

//FxGlyph

FXGlyph::FXGlyph(FXGlyphSet* set, unsigned int chr) {

    this->set = set;
    this->chr = chr;
    
    FT_Face ftface = set->getFTFace();
    
    if(FT_Load_Glyph( ftface, FT_Get_Char_Index( ftface, chr ), FT_LOAD_DEFAULT ))
    throw FXFontException(ftface->family_name);

    FT_Glyph ftglyph;
    
    if(FT_Get_Glyph( ftface->glyph, &ftglyph ))
        throw FXFontException(ftface->family_name);

    FT_Glyph_To_Bitmap( &ftglyph, FT_RENDER_MODE_NORMAL, 0, 1 );

    glyph_bitmap = (FT_BitmapGlyph)ftglyph;
    
    dims    = vec2f( glyph_bitmap->bitmap.width, glyph_bitmap->bitmap.rows);
    corner  = vec2f( glyph_bitmap->left, glyph_bitmap->top);
    advance = vec2f( ftface->glyph->advance.x >> 6, ftface->glyph->advance.y >> 6);

    call_list = 0;
    page = 0;
}

void FXGlyph::compile(FXGlyphPage* page, const vec4f& texcoords) {

    //fprintf(stderr, "%c: %.2f, %.2f, %.2f, %.2f\n", chr, texcoords.x, texcoords.y, texcoords.z, texcoords.w);
    
    this->page = page;
    
    call_list = glGenLists(1);    
    
    glNewList(call_list, GL_COMPILE);

        glBegin(GL_QUADS);
        
        glTexCoord2f(texcoords.x,texcoords.y);
        glVertex2f(0.0f, 0.0f);

        glTexCoord2f(texcoords.z,texcoords.y);
        glVertex2f(dims.x, 0.0f);

        glTexCoord2f(texcoords.z, texcoords.w);
        glVertex2f(dims.x, dims.y);

        glTexCoord2f(texcoords.x,texcoords.w);
        glVertex2f(0.0f, dims.y);

        glEnd();

    glEndList();
}

//FXGlyphPage


FXGlyphPage::FXGlyphPage(int page_width, int page_height) {
    this->page_width  = page_width;
    this->page_height = page_height;

    texture_data = new GLubyte[ page_width * page_height ];
    memset(texture_data, 0, sizeof(texture_data));
    
    needs_update = false;
    textureid = 0;
    glGenTextures(1, &textureid);
    
    max_glyph_height = cursor_x = cursor_y = 0;
}    

FXGlyphPage::~FXGlyphPage() {
    delete[] texture_data;
    if(textureid!=0) glDeleteTextures(1, &textureid);
}

bool FXGlyphPage::addGlyph(FXGlyph* glyph) {

    FT_BitmapGlyph bitmap = glyph->glyph_bitmap;
    
    int corner_x = cursor_x;
    int corner_y = cursor_y;
    
    if(bitmap->bitmap.rows > max_glyph_height) max_glyph_height = bitmap->bitmap.rows;
    
    if(corner_x + bitmap->bitmap.width >= page_width) {
        corner_x = 0;
        corner_y += max_glyph_height + 1;
    
        //bitmap is bigger than the full dimension
        if(bitmap->bitmap.width >= page_width) return false;
    }

    if(corner_y + bitmap->bitmap.rows >= page_height) return false;

    needs_update = true;

    for(int j=0; j < bitmap->bitmap.rows;j++) {
        for(int i=0; i < bitmap->bitmap.width; i++) {
            texture_data[(corner_x+i+(j+corner_y)*page_width)] = bitmap->bitmap.buffer[i + bitmap->bitmap.width*j];
        }
    }

    //fprintf(stderr, "corner_x = %d, corner_y = %d\n", corner_x, corner_y);

    vec4f texcoords = vec4f( corner_x / (float) page_width,
                             corner_y / (float) page_height,
                             (corner_x+bitmap->bitmap.width) / (float) page_width, 
                             (corner_y+bitmap->bitmap.rows) / (float) page_height );
   
    glyph->compile(this, texcoords);
    
    // move cursor for next character
    cursor_x = corner_x + bitmap->bitmap.width + 1;
    cursor_y = corner_y;

    return true;
}

void FXGlyphPage::updateTexture() {
    if(!needs_update) return;
    
    glBindTexture( GL_TEXTURE_2D, textureid);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);

    glTexImage2D( GL_TEXTURE_2D, 0, GL_ALPHA, page_width, page_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, texture_data );

    needs_update = false;
}

//FXGlyphSet

FXGlyphSet::FXGlyphSet(FT_Library freetype, const std::string& fontfile, int size, int dpi) {
    this->freetype = freetype;
    this->fontfile = fontfile;
    this->size     = size;
    this->dpi      = dpi;
    this->ftface   = 0;  
    
    init();
}

FXGlyphSet::~FXGlyphSet() {
    if(ftface!=0) FT_Done_Face(ftface);
    
    for(std::vector<FXGlyphPage*>::iterator it = pages.begin(); it != pages.end(); it++) {
        delete (*it);
    }
    pages.clear();
}

void FXGlyphSet::init() {

    if(FT_New_Face(freetype, fontfile.c_str(), 0, &ftface)) {
        throw FXFontException(fontfile);
    }
    
    int ft_font_size = 64 * size;

    FT_Set_Char_Size( ftface, ft_font_size, ft_font_size, dpi, dpi );

    precache("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ;:'\",<.>/?-_=+!@#$%^&*()\\ ");
}

void FXGlyphSet::precache(const std::string& chars) {

    FTUnicodeStringItr<char> precache_glyphs(chars.c_str());

    unsigned int chr;
   
    //add to bitmap without updating textures until the end
    pre_caching = true;
    
    while (chr = *precache_glyphs++) {
        getGlyph(chr);
    }

    pre_caching = false;

    for(std::vector<FXGlyphPage*>::iterator it = pages.begin(); it != pages.end(); it++) {
        (*it)->updateTexture();
    }
   
}

FXGlyph* FXGlyphSet::getGlyph(unsigned int chr) {
    std::map<unsigned int, FXGlyph*>::iterator it;

    if((it = glyphs.find(chr)) != glyphs.end()) return it->second;

    //if new
    
    FXGlyph* glyph = new FXGlyph(this, chr);
    
    // paint glyph to next page it will fit on
    
    FXGlyphPage* page = 0;
    
    if(!pages.empty()) page = pages.back();
    
    //page is full, create new page
    if(page == 0 || !page->addGlyph(glyph)) {

        //allocate page using maximum allowed texture size
        GLint max_texture_size;        
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
        max_texture_size = std::min( 512, max_texture_size );
        
        page = new FXGlyphPage(max_texture_size, max_texture_size);
        
        pages.push_back(page);

        if(!page->addGlyph(glyph)) {
            throw FXFontException(glyph->set->getFTFace()->family_name);
        }
    }
        
    //update the texture unless this is the precaching process
    if(!pre_caching) page->updateTexture();
    
    glyphs[chr] = glyph;

    return glyph;
}

float FXGlyphSet::getMaxWidth() const {
    return ftface->size->metrics.max_advance / 64.0f;
}

float FXGlyphSet::getMaxHeight() const {
    return (ftface->bbox.yMax - ftface->bbox.yMin) / 64.0f;
}

float FXGlyphSet::getWidth(const std::string& text) {

    FTUnicodeStringItr<char> unicode_text(text.c_str());
     
    float width = 0.0;

    while (unsigned int c = *unicode_text++) {
        FXGlyph* glyph = getGlyph(c);
        width += glyph->getAdvance().x;
    }

    return width;
}

void FXGlyphSet::draw(const std::string& text) {
    
    FTUnicodeStringItr<char> unicode_text(text.c_str());
    
    unsigned int chr;

    GLuint textureid = -1;
    
    while (chr = *unicode_text++) {
        FXGlyph* glyph = getGlyph(chr);

        if(glyph->page->textureid != textureid) {
            textureid = glyph->page->textureid;
            glBindTexture(GL_TEXTURE_2D, textureid);
        }
        
        glPushMatrix();
            glTranslatef(glyph->getCorner().x, -glyph->getCorner().y, 0.0f);
            glyph->draw();
        glPopMatrix();
        
        glTranslatef(glyph->getAdvance().x, glyph->getAdvance().y, 0.0f);
    }
}

//FXFont

FXFont::FXFont() {
    glyphset = 0;
}

FXFont::FXFont(FXGlyphSet* glyphset) {
    this->glyphset = glyphset;
    init();
}

void FXFont::init() {
    shadow          = false;
    shadow_strength = 0.7;
    shadow_offset   = vec2f(1.0, 1.0);

    round           = false;
    align_right     = false;
    align_top       = true;
}

void FXFont::roundCoordinates(bool round) {
    this->round = round;
}

void FXFont::alignRight(bool align_right) {
    this->align_right = align_right;
}

void FXFont::alignTop(bool align_top) {
    this->align_top = align_top;
}

void FXFont::shadowStrength(float s) {
    shadow_strength = s;
}

void FXFont::shadowOffset(float x, float y) {
    shadow_offset = vec2f(x,y);
}

void FXFont::dropShadow(bool shadow) {
    this->shadow = shadow;
}

const std::string& FXFont::getFontFile() const {
    return glyphset->getFontFile();
}

float FXFont::getMaxWidth() const {
    return glyphset->getMaxWidth();
}

float FXFont::getMaxHeight() const {
    return glyphset->getMaxHeight();
}

int FXFont::getFontSize() const {
    return glyphset->getSize();
}

float FXFont::getWidth(const std::string& text) const {
    return glyphset->getWidth(text);   
}

void FXFont::render(float x, float y, const std::string& text) const{

    if(round) {
        x = roundf(x);
        y = roundf(y);
//        fprintf(stderr, "%.2f, %.2f %s\n", x, y, text.c_str());
    }

    glPushMatrix();

       glTranslatef(x,y,0.0f);

       glyphset->draw(text);        

    glPopMatrix();
}

void FXFont::print(float x, float y, const char *str, ...) const{
    char buf[4096];

    va_list vl;

    va_start (vl, str);
    vsnprintf (buf, 4096, str, vl);
    va_end (vl);

    std::string text = std::string(buf);

    draw(x, y, text);
}

void FXFont::draw(float x, float y, const std::string& text) const {

    if(align_right) {
        x -= getWidth(text);
    }

    if(align_top) {
        //fprintf(stderr, "font size = %d, max height = %.2f, ascender = %.2f, descender = %.2f\n", getFontSize(), getMaxHeight(), (glyphset->getFTFace()->ascender / 64.0f), (glyphset->getFTFace()->descender / 64.0f));
        y += getFontSize() + (glyphset->getFTFace()->descender / 64.0f);
    }

    if(shadow) {
        glPushAttrib(GL_CURRENT_BIT);
        vec4f current = display.currentColour();
        glColor4f(0.0f, 0.0f, 0.0f, shadow_strength * current.w);
        render(x + shadow_offset.x, y + shadow_offset.y, text);
        glPopAttrib();
    }

    render(x, y, text);
}

//FXLabel

FXLabel::FXLabel() {
    call_list = 0;
}

FXLabel::FXLabel(const FXFont& font, const std::string& text) {
    call_list = 0;
    setText(font, text);
}

FXLabel::~FXLabel() {
    if(call_list != 0) glDeleteLists(call_list, 1);
}

void FXLabel::setText(const FXFont& font, const std::string& text) {
   
    if(!call_list) call_list = glGenLists(1);
    
    glNewList(call_list, GL_COMPILE);

        font.draw(0.0f, 0.0f, text);
    
    glEndList();
}

// FXFontManager
FXFontManager::FXFontManager() {
    library = 0;
}

void FXFontManager::init() {
    if(FT_Init_FreeType( &library ))
        throw FXFontException("Failed to init FreeType");
}

void FXFontManager::destroy() {
    if(library != 0) FT_Done_FreeType(library);
}

void FXFontManager::setDir(std::string font_dir) {
    this->font_dir = font_dir;
}

void FXFontManager::purge() {

    for(std::map<std::string,fontSizeMap*>::iterator it = fonts.begin(); it!=fonts.end();it++) {
        fontSizeMap* sizemap = it->second;

        for(fontSizeMap::iterator ft_it = sizemap->begin(); ft_it != sizemap->end(); ft_it++) {
            delete ft_it->second;
        }
        delete sizemap;
    }

    fonts.clear();
}

FXFont FXFontManager::grab(std::string font_file, int size, int dpi) {

    char buf[256];

    if(font_dir.size()>0 && font_file[0] != '/') {
        font_file = font_dir + font_file;
    }

    //sprintf(buf, "%s:%i", font_file.c_str(), size);
    //std::string font_key = std::string(buf);

    fontSizeMap* sizemap = fonts[font_file];
    
    if(!sizemap) {
        sizemap = fonts[font_file] = new fontSizeMap;
    }
    
    fontSizeMap::iterator ft_it = sizemap->find(size);   

    FXGlyphSet* glyphset;
    
    if(ft_it == sizemap->end()) {
        glyphset = new FXGlyphSet(library, font_file.c_str(), size, dpi);
        sizemap->insert(std::pair<int,FXGlyphSet*>(size,glyphset));
    } else {
        glyphset = ft_it->second;
    }

    return FXFont(glyphset);
}

