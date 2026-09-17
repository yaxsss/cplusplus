#ifndef TEMPLATE_H
#define TEMPLATE_H
#include <type_traits>

template <class T, class U>
constexpr T narrow_cast(U&& u) noexcept
{
  return static_cast<T>(std::forward<U>(u));
}

constexpr std::size_t dynamic_extent = narrow_cast<std::size_t>(-1);

template <std::size_t From, std::size_t To>
struct is_allowed_extent_conversion
  : std::integral_constant<bool, From == To || To == dynamic_extent>
{
};

// 检查元素是否可以从From转换为To, 这里为什么是{From (*)[], To(*)[]}？
template <class From, class To>
struct is_allowed_element_type_conversion
  : std::integral_constant<bool, std::is_convertible<From (*)[], To(*)[]>::value> 
{
};


template <std::size_t Ext>
class extent_type 
{
public:
  using size_type = std::size_t;
  constexpr extent_type() noexcept = default;
  constexpr explicit extent_type(extent_type<dynamic_extent>); 
  constexpr explicit extent_type(size_type t) {}
  constexpr size_type size() const noexcept{return Ext;}
private:
#if defined (GSL_USE_STATIC_CONSTEXPR_WORKGROUND)
  static constexpr const size_type size_ = Ext;
#else
  static constexpr size_type size_ = Ext;
#endif
};
template<>
class extent_type<dynamic_extent>
{
public:
  using size_type = std::size_t;

  template <size_type Other>
  constexpr explicit extent_type(extent_type<Other> ext) : size_(ext.size()) {}

  constexpr explicit extent_type(size_type size) : size_(size)
  {

  }
  constexpr size_type size() const noexcept{return size_;}
private:
  size_type size_;
};


template <class ElementType, class ExtentType>
class storage_type : public ExtentType
{
  using pointer = ElementType*;
  struct KnownNotNull {
    pointer p;
  };
public: 
  template<class OtherExtentType>
  constexpr storage_type(KnownNotNull data, OtherExtentType ext)
    : ExtentType(ext), data_(data.p)
  {}
  constexpr pointer data() const noexcept {return data_;} 

private:
  pointer data_;
};


#endif