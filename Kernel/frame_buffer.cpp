#include "error.hpp"
#include "frame_buffer.hpp"
#include "frame_buffer_config.hpp"

namespace {
  int BytesPerPixel(PixelFormat format) {
    auto bits = BitsPerPixel(format);
    if(bits < 0) {
      return -1;
    }
    
    return (bits + 7) / 8;
  }

  uint8_t* FrameAddrAt(Vector2D<int> pos, const FrameBufferConfig& config) {
    auto bytes = BytesPerPixel(config.pixel_format);
    if(bytes < 0) {
      return nullptr;
    }

    return config.frame_buffer + bytes * (config.pixels_per_scanline * pos.y + pos.x);
  }

  int BytesPerScanLine(const FrameBufferConfig& config) {
    auto bytes = BytesPerPixel(config.pixel_format);
    if(bytes < 0) {
      return -1;
    }

    return bytes * config.pixels_per_scanline;
  }

  Vector2D<int> FrameBufferSize(const FrameBufferConfig& config) {
    return {
      static_cast<int>(config.horizontal_resolution),
      static_cast<int>(config.vertical_resolution)
    };
  }
}

Error FrameBuffer::Initialize(const FrameBufferConfig& config) {
  config_ = config;

  const auto bits_per_pixel = BitsPerPixel(config_.pixel_format);
  if(bits_per_pixel <= 0) {
    return MAKE_ERROR(Error::kUnknownPixelFormat);
  }

  if(config_.frame_buffer) {
    buffer_.resize(0);
  } else {
    buffer_.resize(((bits_per_pixel + 7) / 8) * config_.horizontal_resolution * config_.vertical_resolution);
    config_.frame_buffer = buffer_.data();  
    config_.pixels_per_scanline = config_.horizontal_resolution;
  }

  switch(config_.pixel_format) {
    case kPixelRGBResv8BitPerColor:
      writer_ = std::make_unique<RGBResv8BitPerColorPixelWriter>(config_);
      break;
    case kPixelBGRResv8BitPerColor:
      writer_ = std::make_unique<BGRResv8BitPerColorPixelWriter>(config_);
      break;
    default:
      return MAKE_ERROR(Error::kUnknownPixelFormat);
  }

  return MAKE_ERROR(Error::kSuccess);
}

Error FrameBuffer::Copy(Vector2D<int> dst_pos, const FrameBuffer& src, Rectangle<int> src_area) {
  if(config_.pixel_format != src.config_.pixel_format) {
    return MAKE_ERROR(Error::kUnknownPixelFormat);
  }

  const auto bits_per_pixel = BitsPerPixel(config_.pixel_format);
  if(bits_per_pixel <= 0) {
    return MAKE_ERROR(Error::kUnknownPixelFormat);
  }

  const Rectangle<int> src_area_shifted{dst_pos, src_area.size};
  const Rectangle<int> src_outline{dst_pos - src_area.pos, FrameBufferSize(src.config_)};
  const Rectangle<int> dst_outline{{0, 0}, FrameBufferSize(config_)};
  const auto copy_area = dst_outline & src_outline & src_area_shifted;  
  const auto src_start_pos = copy_area.pos - (dst_pos - src_area.pos);

  uint8_t* dst_buf = FrameAddrAt(copy_area.pos, config_);
  const uint8_t* src_buf = FrameAddrAt(src_start_pos, src.config_);  
  
  auto bytes_per_pixel = BytesPerPixel(config_.pixel_format);
  for(int dy = 0; dy < copy_area.size.y; dy++) {
    memcpy(dst_buf, src_buf, bytes_per_pixel * copy_area.size.x);
    dst_buf += bytes_per_pixel * config_.pixels_per_scanline;
    src_buf += bytes_per_pixel * src.config_.pixels_per_scanline;
  }

  return MAKE_ERROR(Error::kSuccess);
}

void FrameBuffer::Move(Vector2D<int> dst_pos, const Rectangle<int>& src) {
  const auto bytes_per_pixel = BytesPerPixel(config_.pixel_format);
  const auto bytes_per_scanline = BytesPerScanLine(config_);

  if(dst_pos.y < src.pos.y) {
    uint8_t* dst_buf = FrameAddrAt(dst_pos, config_);
    const uint8_t* src_buf = FrameAddrAt(src.pos, config_);

    for(int y = 0; y < src.size.y; y++) {
      memcpy(dst_buf, src_buf, bytes_per_pixel * src.size.x);
      dst_buf += bytes_per_scanline;
      src_buf += bytes_per_scanline;
    }
  } else {
    uint8_t* dst_buf = FrameAddrAt(dst_pos + Vector2D<int>{0, src.size.y - 1}, config_);
    const uint8_t* src_buf = FrameAddrAt(src.pos + Vector2D<int>{0, src.size.y - 1}, config_);

    for (int y = 0; y < src.size.y; y++) {
      memcpy(dst_buf, src_buf, bytes_per_pixel * src.size.x);
      dst_buf -= bytes_per_scanline;      
      src_buf -= bytes_per_scanline;      
    }
  }
}

int BitsPerPixel(PixelFormat format) {
  switch(format) {
    case kPixelRGBResv8BitPerColor: return 32;
    case kPixelBGRResv8BitPerColor: return 32;
  }

  return -1;
}