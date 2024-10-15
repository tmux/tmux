/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#ifndef _INC_COMUTIL
#define _INC_COMUTIL

#include <ole2.h>
#include <stdio.h>

#ifndef _COM_ASSERT
#define _COM_ASSERT(x) ((void)0)
#endif

#define _COM_MEMCPY_S(dest,destsize,src,count) memcpy(dest,src,count)

/* Use of wsprintf might be impossible, if strsafe.h is included. */
#ifndef __STDC_SECURE_LIB__
#define _COM_PRINTF_S_1(dest,destsize,format,arg1) wsprintf(dest,format,arg1)
#elif defined(UNICODE)
#define _COM_PRINTF_S_1(dest,destsize,format,arg1) swprintf_s(dest,destsize,format,arg1)
#else
#define _COM_PRINTF_S_1(dest,destsize,format,arg1) sprintf_s(dest,destsize,format,arg1)
#endif

#ifdef __cplusplus

#pragma push_macro("new")
#undef new

#ifndef WINAPI
#if defined(_ARM_)
#define WINAPI
#else
#define WINAPI __stdcall
#endif
#endif

class _com_error;

void WINAPI _com_issue_error(HRESULT);

class _bstr_t;
class _variant_t;

namespace _com_util {
  inline void CheckError(HRESULT hr) {
    if(FAILED(hr)) { _com_issue_error(hr); }
  }
}

namespace _com_util {
  BSTR WINAPI ConvertStringToBSTR(const char *pSrc);
  char *WINAPI ConvertBSTRToString(BSTR pSrc);
}

class _bstr_t {
public:
  _bstr_t() throw();
  _bstr_t(const _bstr_t &s) throw();
  _bstr_t(const char *s);
  _bstr_t(const wchar_t *s);
  _bstr_t(const _variant_t &var);
  _bstr_t(BSTR bstr,bool fCopy);
  ~_bstr_t() throw();
  _bstr_t &operator=(const _bstr_t &s) throw();
  _bstr_t &operator=(const char *s);
  _bstr_t &operator=(const wchar_t *s);
  _bstr_t &operator=(const _variant_t &var);
  _bstr_t &operator+=(const _bstr_t &s);
  _bstr_t operator+(const _bstr_t &s) const;
  friend _bstr_t operator+(const char *s1,const _bstr_t &s2);
  friend _bstr_t operator+(const wchar_t *s1,const _bstr_t &s2);
  operator const wchar_t *() const throw();
  operator wchar_t *() const throw();
  operator const char *() const;
  operator char *() const;
  bool operator!() const throw();
  bool operator==(const _bstr_t &str) const throw();
  bool operator!=(const _bstr_t &str) const throw();
  bool operator<(const _bstr_t &str) const throw();
  bool operator>(const _bstr_t &str) const throw();
  bool operator<=(const _bstr_t &str) const throw();
  bool operator>=(const _bstr_t &str) const throw();
  BSTR copy(bool fCopy = true) const;
  unsigned int length() const throw();
  void Assign(BSTR s);
  BSTR &GetBSTR();
  BSTR *GetAddress();
  void Attach(BSTR s);
  BSTR Detach() throw();
private:
  class Data_t {
  public:
    Data_t(const char *s);
    Data_t(const wchar_t *s);
    Data_t(BSTR bstr,bool fCopy);
    Data_t(const _bstr_t &s1,const _bstr_t &s2);
    unsigned __LONG32 AddRef() throw();
    unsigned __LONG32 Release() throw();
    unsigned __LONG32 RefCount() const throw();
    operator const wchar_t *() const throw();
    operator const char *() const;
    const wchar_t *GetWString() const throw();
    wchar_t *&GetWString() throw();
    const char *GetString() const;
    BSTR Copy() const;
    void Assign(BSTR s);
    void Attach(BSTR s) throw();
    unsigned int Length() const throw();
    int Compare(const Data_t &str) const throw();
    void *operator new(size_t sz);
  private:
    BSTR m_wstr;
    mutable char *m_str;
    unsigned __LONG32 m_RefCount;
    Data_t() throw();
    Data_t(const Data_t &s) throw();
    ~Data_t() throw();
    void _Free() throw();
  };
private:
  Data_t *m_Data;
private:
  void _AddRef() throw();
  void _Free() throw();
  int _Compare(const _bstr_t &str) const throw();
};

inline _bstr_t::_bstr_t() throw() : m_Data(NULL) { }

inline _bstr_t::_bstr_t(const _bstr_t &s) throw() : m_Data(s.m_Data) { _AddRef(); }

inline _bstr_t::_bstr_t(const char *s) : m_Data(new Data_t(s)) {
  if(!m_Data) { _com_issue_error(E_OUTOFMEMORY); }
}

inline _bstr_t::_bstr_t(const wchar_t *s) : m_Data(new Data_t(s)) {
  if(!m_Data) { _com_issue_error(E_OUTOFMEMORY); }
}

inline _bstr_t::_bstr_t(BSTR bstr,bool fCopy) : m_Data(new Data_t(bstr,fCopy)) {
  if(!m_Data) { _com_issue_error(E_OUTOFMEMORY); }
}

inline _bstr_t::~_bstr_t() throw() { _Free(); }

inline _bstr_t &_bstr_t::operator=(const _bstr_t &s) throw() {
  if(this!=&s) {
    _Free();
    m_Data = s.m_Data;
    _AddRef();
  }
  return *this;
}

inline _bstr_t &_bstr_t::operator=(const char *s) {
  _COM_ASSERT(!s || static_cast<const char *>(*this)!=s);
  if(!s || static_cast<const char *>(*this)!=s) {
    _Free();
    m_Data = new Data_t(s);
    if(!m_Data) { _com_issue_error(E_OUTOFMEMORY); }
  }
  return *this;
}

inline _bstr_t &_bstr_t::operator=(const wchar_t *s) {
  _COM_ASSERT(!s || static_cast<const wchar_t *>(*this)!=s);
  if(!s || static_cast<const wchar_t *>(*this)!=s) {
    _Free();
    m_Data = new Data_t(s);
    if(!m_Data) { _com_issue_error(E_OUTOFMEMORY); }
  }
  return *this;
}

inline _bstr_t &_bstr_t::operator+=(const _bstr_t &s) {
  Data_t *newData = new Data_t(*this,s);
  if(!newData) { _com_issue_error(E_OUTOFMEMORY); }
  else {
    _Free();
    m_Data = newData;
  }
  return *this;
}

inline _bstr_t _bstr_t::operator+(const _bstr_t &s) const {
  _bstr_t b = *this;
  b += s;
  return b;
}

inline _bstr_t operator+(const char *s1,const _bstr_t &s2) {
  _bstr_t b = s1;
  b += s2;
  return b;
}

inline _bstr_t operator+(const wchar_t *s1,const _bstr_t &s2) {
  _bstr_t b = s1;
  b += s2;
  return b;
}

inline _bstr_t::operator const wchar_t *() const throw() { return (m_Data!=NULL) ? m_Data->GetWString() : NULL; }
inline _bstr_t::operator wchar_t *() const throw() { return const_cast<wchar_t *>((m_Data!=NULL) ? m_Data->GetWString() : NULL); }
inline _bstr_t::operator const char *() const { return (m_Data!=NULL) ? m_Data->GetString() : NULL; }
inline _bstr_t::operator char *() const { return const_cast<char *>((m_Data!=NULL) ? m_Data->GetString() : NULL); }
inline bool _bstr_t::operator!() const throw() { return (m_Data!=NULL) ? !m_Data->GetWString() : true; }
inline bool _bstr_t::operator==(const _bstr_t &str) const throw() { return _Compare(str)==0; }
inline bool _bstr_t::operator!=(const _bstr_t &str) const throw() { return _Compare(str)!=0; }
inline bool _bstr_t::operator<(const _bstr_t &str) const throw() { return _Compare(str)<0; }
inline bool _bstr_t::operator>(const _bstr_t &str) const throw() { return _Compare(str)>0; }
inline bool _bstr_t::operator<=(const _bstr_t &str) const throw() { return _Compare(str)<=0; }
inline bool _bstr_t::operator>=(const _bstr_t &str) const throw() { return _Compare(str)>=0; }
inline BSTR _bstr_t::copy(bool fCopy) const { return (m_Data!=NULL) ? (fCopy ? m_Data->Copy() : m_Data->GetWString()) : NULL; }
inline unsigned int _bstr_t::length() const throw() { return (m_Data!=NULL) ? m_Data->Length() : 0; }
inline void _bstr_t::Assign(BSTR s) {
  _COM_ASSERT(!s || !m_Data || m_Data->GetWString()!=s);
  if(!s || !m_Data || m_Data->GetWString()!=s) {
    _Free();
    m_Data = new Data_t(s,TRUE);
    if(!m_Data) { _com_issue_error(E_OUTOFMEMORY); }
  }
}

inline BSTR &_bstr_t::GetBSTR() {
  if(!m_Data) {
    m_Data = new Data_t(0,FALSE);
    if(!m_Data) { _com_issue_error(E_OUTOFMEMORY); }
  }
  return m_Data->GetWString();
}

inline BSTR *_bstr_t::GetAddress() {
  Attach(0);
  return &m_Data->GetWString();
}

inline void _bstr_t::Attach(BSTR s) {
  _Free();
  m_Data = new Data_t(s,FALSE);
  if(!m_Data) { _com_issue_error(E_OUTOFMEMORY); }
}

inline BSTR _bstr_t::Detach() throw () {
  _COM_ASSERT(m_Data!=NULL && m_Data->RefCount()==1);
  if(m_Data!=NULL && m_Data->RefCount()==1) {
    BSTR b = m_Data->GetWString();
    m_Data->GetWString() = NULL;
    _Free();
    return b;
  } else {
    _com_issue_error(E_POINTER);
    return NULL;
  }
}

inline void _bstr_t::_AddRef() throw() {
  if(m_Data!=NULL) m_Data->AddRef();
}

inline void _bstr_t::_Free() throw() {
  if(m_Data!=NULL) {
    m_Data->Release();
    m_Data = NULL;
  }
}

inline int _bstr_t::_Compare(const _bstr_t &str) const throw() {
  if(m_Data==str.m_Data) return 0;
  if(!m_Data) return -1;
  if(!str.m_Data) return 1;
  return m_Data->Compare(*str.m_Data);
}

inline _bstr_t::Data_t::Data_t(const char *s) : m_str(NULL),m_RefCount(1) {
  m_wstr = _com_util::ConvertStringToBSTR(s);
}

inline _bstr_t::Data_t::Data_t(const wchar_t *s) : m_str(NULL),m_RefCount(1) {
  m_wstr = ::SysAllocString(s);
  if(!m_wstr && s!=NULL) { _com_issue_error(E_OUTOFMEMORY); }
}

inline _bstr_t::Data_t::Data_t(BSTR bstr,bool fCopy) : m_str(NULL),m_RefCount(1) {
  if(fCopy && bstr!=NULL) {
    m_wstr = ::SysAllocStringByteLen(reinterpret_cast<char *>(bstr),::SysStringByteLen(bstr));
    if(!m_wstr) { _com_issue_error(E_OUTOFMEMORY); }
  } else m_wstr = bstr;
}

inline _bstr_t::Data_t::Data_t(const _bstr_t &s1,const _bstr_t &s2) : m_str(NULL),m_RefCount(1) {
  const unsigned int l1 = s1.length();
  const unsigned int l2 = s2.length();
  m_wstr = ::SysAllocStringByteLen(NULL,(l1 + l2) *sizeof(wchar_t));
  if(!m_wstr) {
    _com_issue_error(E_OUTOFMEMORY);
    return;
  }
  const wchar_t *wstr1 = static_cast<const wchar_t *>(s1);
  if(wstr1!=NULL) {
    _COM_MEMCPY_S(m_wstr,(l1 + l2 + 1) *sizeof(wchar_t),wstr1,(l1 + 1) *sizeof(wchar_t));
  }
  const wchar_t *wstr2 = static_cast<const wchar_t *>(s2);
  if(wstr2!=NULL) {
    _COM_MEMCPY_S(m_wstr + l1,(l2 + 1) *sizeof(wchar_t),wstr2,(l2 + 1) *sizeof(wchar_t));
  }
}

inline unsigned __LONG32 _bstr_t::Data_t::AddRef() throw() {
  InterlockedIncrement(reinterpret_cast<LONG*>(&m_RefCount));
  return m_RefCount;
}

inline unsigned __LONG32 _bstr_t::Data_t::Release() throw() {
  unsigned __LONG32 cRef = InterlockedDecrement(reinterpret_cast<LONG*>(&m_RefCount));
  if(cRef==0) delete this;
  return cRef;
}

inline unsigned __LONG32 _bstr_t::Data_t::RefCount() const throw() { return m_RefCount; }
inline _bstr_t::Data_t::operator const wchar_t *() const throw() { return m_wstr; }
inline _bstr_t::Data_t::operator const char *() const { return GetString(); }
inline const wchar_t *_bstr_t::Data_t::GetWString() const throw() { return m_wstr; }
inline wchar_t *&_bstr_t::Data_t::GetWString() throw() { return m_wstr; }
inline const char *_bstr_t::Data_t::GetString() const {
  if(!m_str) m_str = _com_util::ConvertBSTRToString(m_wstr);
  return m_str;
}
inline BSTR _bstr_t::Data_t::Copy() const {
  if(m_wstr!=NULL) {
    BSTR bstr = ::SysAllocStringByteLen(reinterpret_cast<char *>(m_wstr),::SysStringByteLen(m_wstr));
    if(!bstr) { _com_issue_error(E_OUTOFMEMORY); }
    return bstr;
  }
  return NULL;
}
inline void _bstr_t::Data_t::Assign(BSTR s) {
  _Free();
  if(s!=NULL) {
    m_wstr = ::SysAllocStringByteLen(reinterpret_cast<char *>(s),::SysStringByteLen(s));
    m_str = 0;
  }
}
inline void _bstr_t::Data_t::Attach(BSTR s) throw() {
  _Free();
  m_wstr = s;
  m_str = 0;
  m_RefCount = 1;
}
inline unsigned int _bstr_t::Data_t::Length() const throw() { return m_wstr ? ::SysStringLen(m_wstr) : 0; }
inline int _bstr_t::Data_t::Compare(const _bstr_t::Data_t &str) const throw() {
  if(!m_wstr) return str.m_wstr ? -1 : 0;
  if(!str.m_wstr) return 1;
  const unsigned int l1 = ::SysStringLen(m_wstr);
  const unsigned int l2 = ::SysStringLen(str.m_wstr);
  unsigned int len = l1;
  if(len>l2) len = l2;
  BSTR bstr1 = m_wstr;
  BSTR bstr2 = str.m_wstr;
  while (len-->0) {
    if(*bstr1++!=*bstr2++) return bstr1[-1] - bstr2[-1];
  }
  return (l1<l2) ? -1 : (l1==l2) ? 0 : 1;
}

#ifdef _COM_OPERATOR_NEW_THROWS
inline void *_bstr_t::Data_t::operator new(size_t sz) {
  try {
    return ::operator new(sz);
  } catch (...) {
    return NULL;
  }
}
#else
inline void *_bstr_t::Data_t::operator new(size_t sz) {
  return ::operator new(sz);
}
#endif

inline _bstr_t::Data_t::~Data_t() throw() { _Free(); }
inline void _bstr_t::Data_t::_Free() throw() {
  if(m_wstr!=NULL) ::SysFreeString(m_wstr);
  if(m_str!=NULL) delete [] m_str;
}

class _variant_t : public ::tagVARIANT {
public:
  _variant_t() throw();
  _variant_t(const VARIANT &varSrc);
  _variant_t(const VARIANT *pSrc);
  _variant_t(const _variant_t &varSrc);
  _variant_t(VARIANT &varSrc,bool fCopy);
  _variant_t(short sSrc,VARTYPE vtSrc = VT_I2);
  _variant_t(__LONG32 lSrc,VARTYPE vtSrc = VT_I4);
  _variant_t(float fltSrc) throw();
  _variant_t(double dblSrc,VARTYPE vtSrc = VT_R8);
  _variant_t(const CY &cySrc) throw();
  _variant_t(const _bstr_t &bstrSrc);
  _variant_t(const wchar_t *pSrc);
  _variant_t(const char *pSrc);
  _variant_t(IDispatch *pSrc,bool fAddRef = true) throw();
  _variant_t(bool boolSrc) throw();
  _variant_t(IUnknown *pSrc,bool fAddRef = true) throw();
  _variant_t(const DECIMAL &decSrc) throw();
  _variant_t(BYTE bSrc) throw();
  _variant_t(char cSrc) throw();
  _variant_t(unsigned short usSrc) throw();
  _variant_t(unsigned __LONG32 ulSrc) throw();
#ifndef __CYGWIN__
  _variant_t(int iSrc) throw();
  _variant_t(unsigned int uiSrc) throw();
#endif
  __MINGW_EXTENSION _variant_t(__int64 i8Src) throw();
  __MINGW_EXTENSION _variant_t(unsigned __int64 ui8Src) throw();
  ~_variant_t() throw();
  operator short() const;
  operator __LONG32() const;
  operator float() const;
  operator double() const;
  operator CY() const;
  operator _bstr_t() const;
  operator IDispatch*() const;
  operator bool() const;
  operator IUnknown*() const;
  operator DECIMAL() const;
  operator BYTE() const;
  operator VARIANT() const throw();
  operator char() const;
  operator unsigned short() const;
  operator unsigned __LONG32() const;
#ifndef __CYGWIN__
  operator int() const;
  operator unsigned int() const;
#endif
  __MINGW_EXTENSION operator __int64() const;
  __MINGW_EXTENSION operator unsigned __int64() const;
  _variant_t &operator=(const VARIANT &varSrc);
  _variant_t &operator=(const VARIANT *pSrc);
  _variant_t &operator=(const _variant_t &varSrc);
  _variant_t &operator=(short sSrc);
  _variant_t &operator=(__LONG32 lSrc);
  _variant_t &operator=(float fltSrc);
  _variant_t &operator=(double dblSrc);
  _variant_t &operator=(const CY &cySrc);
  _variant_t &operator=(const _bstr_t &bstrSrc);
  _variant_t &operator=(const wchar_t *pSrc);
  _variant_t &operator=(const char *pSrc);
  _variant_t &operator=(IDispatch *pSrc);
  _variant_t &operator=(bool boolSrc);
  _variant_t &operator=(IUnknown *pSrc);
  _variant_t &operator=(const DECIMAL &decSrc);
  _variant_t &operator=(BYTE bSrc);
  _variant_t &operator=(char cSrc);
  _variant_t &operator=(unsigned short usSrc);
  _variant_t &operator=(unsigned __LONG32 ulSrc);
#ifndef __CYGWIN__
  _variant_t &operator=(int iSrc);
  _variant_t &operator=(unsigned int uiSrc);
#endif
  __MINGW_EXTENSION _variant_t &operator=(__int64 i8Src);
  __MINGW_EXTENSION _variant_t &operator=(unsigned __int64 ui8Src);
  bool operator==(const VARIANT &varSrc) const throw();
  bool operator==(const VARIANT *pSrc) const throw();
  bool operator!=(const VARIANT &varSrc) const throw();
  bool operator!=(const VARIANT *pSrc) const throw();
  void Clear();
  void Attach(VARIANT &varSrc);
  VARIANT Detach();
  VARIANT &GetVARIANT() throw();
  VARIANT *GetAddress();
  void ChangeType(VARTYPE vartype,const _variant_t *pSrc = NULL);
  void SetString(const char *pSrc);
};

inline _variant_t::_variant_t() throw() { ::VariantInit(this); }
inline _variant_t::_variant_t(const VARIANT &varSrc) {
  ::VariantInit(this);
  _com_util::CheckError(::VariantCopy(this,const_cast<VARIANT*>(&varSrc)));
}
inline _variant_t::_variant_t(const VARIANT *pSrc) {
  if(!pSrc) { _com_issue_error(E_POINTER); }
  else {
    ::VariantInit(this);
    _com_util::CheckError(::VariantCopy(this,const_cast<VARIANT*>(pSrc)));
  }
}
inline _variant_t::_variant_t(const _variant_t &varSrc) {
  ::VariantInit(this);
  _com_util::CheckError(::VariantCopy(this,const_cast<VARIANT*>(static_cast<const VARIANT*>(&varSrc))));
}
inline _variant_t::_variant_t(VARIANT &varSrc,bool fCopy) {
  if(fCopy) {
    ::VariantInit(this);
    _com_util::CheckError(::VariantCopy(this,&varSrc));
  } else {
    _COM_MEMCPY_S(this,sizeof(varSrc),&varSrc,sizeof(varSrc));
    V_VT(&varSrc) = VT_EMPTY;
  }
}
inline _variant_t::_variant_t(short sSrc,VARTYPE vtSrc) {
  if((vtSrc!=VT_I2) && (vtSrc!=VT_BOOL)) {
    _com_issue_error(E_INVALIDARG);
    return;
  }
  if(vtSrc==VT_BOOL) {
    V_VT(this) = VT_BOOL;
    V_BOOL(this) = (sSrc ? VARIANT_TRUE : VARIANT_FALSE);
  } else {
    V_VT(this) = VT_I2;
    V_I2(this) = sSrc;
  }
}
inline _variant_t::_variant_t(__LONG32 lSrc,VARTYPE vtSrc) {
  if((vtSrc!=VT_I4) && (vtSrc!=VT_ERROR) && (vtSrc!=VT_BOOL)) {
    _com_issue_error(E_INVALIDARG);
    return;
  }
  if(vtSrc==VT_ERROR) {
    V_VT(this) = VT_ERROR;
    V_ERROR(this) = lSrc;
  } else if(vtSrc==VT_BOOL) {
    V_VT(this) = VT_BOOL;
    V_BOOL(this) = (lSrc ? VARIANT_TRUE : VARIANT_FALSE);
  } else {
    V_VT(this) = VT_I4;
    V_I4(this) = lSrc;
  }
}
inline _variant_t::_variant_t(float fltSrc) throw() {
  V_VT(this) = VT_R4;
  V_R4(this) = fltSrc;
}

inline _variant_t::_variant_t(double dblSrc,VARTYPE vtSrc) {
  if((vtSrc!=VT_R8) && (vtSrc!=VT_DATE)) {
    _com_issue_error(E_INVALIDARG);
    return;
  }
  if(vtSrc==VT_DATE) {
    V_VT(this) = VT_DATE;
    V_DATE(this) = dblSrc;
  } else {
    V_VT(this) = VT_R8;
    V_R8(this) = dblSrc;
  }
}
inline _variant_t::_variant_t(const CY &cySrc) throw() {
  V_VT(this) = VT_CY;
  V_CY(this) = cySrc;
}
inline _variant_t::_variant_t(const _bstr_t &bstrSrc) {
  V_VT(this) = VT_BSTR;
  BSTR bstr = static_cast<wchar_t *>(bstrSrc);
  if(!bstr) V_BSTR(this) = NULL;
  else {
    V_BSTR(this) = ::SysAllocStringByteLen(reinterpret_cast<char *>(bstr),::SysStringByteLen(bstr));
    if(!(V_BSTR(this))) { _com_issue_error(E_OUTOFMEMORY); }
  }
}
inline _variant_t::_variant_t(const wchar_t *pSrc) {
  V_VT(this) = VT_BSTR;
  V_BSTR(this) = ::SysAllocString(pSrc);
  if(!(V_BSTR(this)) && pSrc!=NULL) { _com_issue_error(E_OUTOFMEMORY); }
}
inline _variant_t::_variant_t(const char *pSrc) {
  V_VT(this) = VT_BSTR;
  V_BSTR(this) = _com_util::ConvertStringToBSTR(pSrc);
}
inline _variant_t::_variant_t(IDispatch *pSrc,bool fAddRef) throw() {
  V_VT(this) = VT_DISPATCH;
  V_DISPATCH(this) = pSrc;
  if(fAddRef && V_DISPATCH(this)!=NULL) V_DISPATCH(this)->AddRef();
}
inline _variant_t::_variant_t(bool boolSrc) throw() {
  V_VT(this) = VT_BOOL;
  V_BOOL(this) = (boolSrc ? VARIANT_TRUE : VARIANT_FALSE);
}
inline _variant_t::_variant_t(IUnknown *pSrc,bool fAddRef) throw() {
  V_VT(this) = VT_UNKNOWN;
  V_UNKNOWN(this) = pSrc;
  if(fAddRef && V_UNKNOWN(this)!=NULL) V_UNKNOWN(this)->AddRef();
}
inline _variant_t::_variant_t(const DECIMAL &decSrc) throw() {
  V_DECIMAL(this) = decSrc;
  V_VT(this) = VT_DECIMAL;
}
inline _variant_t::_variant_t(BYTE bSrc) throw() {
  V_VT(this) = VT_UI1;
  V_UI1(this) = bSrc;
}
inline _variant_t::_variant_t(char cSrc) throw() {
  V_VT(this) = VT_I1;
  V_I1(this) = cSrc;
}
inline _variant_t::_variant_t(unsigned short usSrc) throw() {
  V_VT(this) = VT_UI2;
  V_UI2(this) = usSrc;
}
inline _variant_t::_variant_t(unsigned __LONG32 ulSrc) throw() {
  V_VT(this) = VT_UI4;
  V_UI4(this) = ulSrc;
}
#ifndef __CYGWIN__
inline _variant_t::_variant_t(int iSrc) throw() {
  V_VT(this) = VT_INT;
  V_INT(this) = iSrc;
}
inline _variant_t::_variant_t(unsigned int uiSrc) throw() {
  V_VT(this) = VT_UINT;
  V_UINT(this) = uiSrc;
}
#endif
__MINGW_EXTENSION inline _variant_t::_variant_t(__int64 i8Src) throw() {
  V_VT(this) = VT_I8;
  V_I8(this) = i8Src;
}
__MINGW_EXTENSION inline _variant_t::_variant_t(unsigned __int64 ui8Src) throw() {
  V_VT(this) = VT_UI8;
  V_UI8(this) = ui8Src;
}
inline _variant_t::operator short() const {
  if(V_VT(this)==VT_I2) return V_I2(this);
  _variant_t varDest;
  varDest.ChangeType(VT_I2,this);
  return V_I2(&varDest);
}
inline _variant_t::operator __LONG32() const {
  if(V_VT(this)==VT_I4) return V_I4(this);
  _variant_t varDest;
  varDest.ChangeType(VT_I4,this);
  return V_I4(&varDest);
}

inline _variant_t::operator float() const {
  if(V_VT(this)==VT_R4) return V_R4(this);
  _variant_t varDest;
  varDest.ChangeType(VT_R4,this);
  return V_R4(&varDest);
}

inline _variant_t::operator double() const {
  if(V_VT(this)==VT_R8) return V_R8(this);
  _variant_t varDest;
  varDest.ChangeType(VT_R8,this);
  return V_R8(&varDest);
}

inline _variant_t::operator CY() const {
  if(V_VT(this)==VT_CY) return V_CY(this);
  _variant_t varDest;
  varDest.ChangeType(VT_CY,this);
  return V_CY(&varDest);
}

inline _variant_t::operator _bstr_t() const {
  if(V_VT(this)==VT_BSTR) return V_BSTR(this);
  _variant_t varDest;
  varDest.ChangeType(VT_BSTR,this);
  return V_BSTR(&varDest);
}

inline _variant_t::operator IDispatch*() const {
  if(V_VT(this)==VT_DISPATCH) {
    if(V_DISPATCH(this)!=NULL) V_DISPATCH(this)->AddRef();
    return V_DISPATCH(this);
  }
  _variant_t varDest;
  varDest.ChangeType(VT_DISPATCH,this);
  if(V_DISPATCH(&varDest)!=NULL) V_DISPATCH(&varDest)->AddRef();
  return V_DISPATCH(&varDest);
}
inline _variant_t::operator bool() const {
  if(V_VT(this)==VT_BOOL) return V_BOOL(this) ? true : false;
  _variant_t varDest;
  varDest.ChangeType(VT_BOOL,this);
  return (V_BOOL(&varDest)==VARIANT_TRUE) ? true : false;
}

inline _variant_t::operator IUnknown*() const {
  if(V_VT(this)==VT_UNKNOWN) {
    if(V_UNKNOWN(this)!=NULL) V_UNKNOWN(this)->AddRef();
    return V_UNKNOWN(this);
  }
  _variant_t varDest;
  varDest.ChangeType(VT_UNKNOWN,this);
  if(V_UNKNOWN(&varDest)!=NULL) V_UNKNOWN(&varDest)->AddRef();
  return V_UNKNOWN(&varDest);
}
inline _variant_t::operator DECIMAL() const {
  if(V_VT(this)==VT_DECIMAL) return V_DECIMAL(this);
  _variant_t varDest;
  varDest.ChangeType(VT_DECIMAL,this);
  return V_DECIMAL(&varDest);
}
inline _variant_t::operator BYTE() const {
  if(V_VT(this)==VT_UI1) return V_UI1(this);
  _variant_t varDest;
  varDest.ChangeType(VT_UI1,this);
  return V_UI1(&varDest);
}
inline _variant_t::operator VARIANT() const throw() { return *(VARIANT*) this; }
inline _variant_t::operator char() const {
  if(V_VT(this)==VT_I1) return V_I1(this);
  _variant_t varDest;
  varDest.ChangeType(VT_I1,this);
  return V_I1(&varDest);
}

inline _variant_t::operator unsigned short() const {
  if(V_VT(this)==VT_UI2) return V_UI2(this);
  _variant_t varDest;
  varDest.ChangeType(VT_UI2,this);
  return V_UI2(&varDest);
}

inline _variant_t::operator unsigned __LONG32() const {
  if(V_VT(this)==VT_UI4) return V_UI4(this);
  _variant_t varDest;
  varDest.ChangeType(VT_UI4,this);
  return V_UI4(&varDest);
}
#ifndef __CYGWIN__
inline _variant_t::operator int() const {
  if(V_VT(this)==VT_INT) return V_INT(this);
  _variant_t varDest;
  varDest.ChangeType(VT_INT,this);
  return V_INT(&varDest);
}
inline _variant_t::operator unsigned int() const {
  if(V_VT(this)==VT_UINT) return V_UINT(this);
  _variant_t varDest;
  varDest.ChangeType(VT_UINT,this);
  return V_UINT(&varDest);
}
#endif
__MINGW_EXTENSION inline _variant_t::operator __int64() const {
  if(V_VT(this)==VT_I8) return V_I8(this);
  _variant_t varDest;
  varDest.ChangeType(VT_I8,this);
  return V_I8(&varDest);
}
__MINGW_EXTENSION inline _variant_t::operator unsigned __int64() const {
  if(V_VT(this)==VT_UI8) return V_UI8(this);
  _variant_t varDest;
  varDest.ChangeType(VT_UI8,this);
  return V_UI8(&varDest);
}
inline _variant_t &_variant_t::operator=(const VARIANT &varSrc) {
  _com_util::CheckError(::VariantCopy(this,const_cast<VARIANT*>(&varSrc)));
  return *this;
}
inline _variant_t &_variant_t::operator=(const VARIANT *pSrc) {
  if(!pSrc) { _com_issue_error(E_POINTER); }
  else { _com_util::CheckError(::VariantCopy(this,const_cast<VARIANT*>(pSrc))); }
  return *this;
}
inline _variant_t &_variant_t::operator=(const _variant_t &varSrc) {
  _com_util::CheckError(::VariantCopy(this,const_cast<VARIANT*>(static_cast<const VARIANT*>(&varSrc))));
  return *this;
}
inline _variant_t &_variant_t::operator=(short sSrc) {
  if(V_VT(this)==VT_I2) V_I2(this) = sSrc;
  else if(V_VT(this)==VT_BOOL) V_BOOL(this) = (sSrc ? VARIANT_TRUE : VARIANT_FALSE);
  else {
    Clear();
    V_VT(this) = VT_I2;
    V_I2(this) = sSrc;
  }
  return *this;
}
inline _variant_t &_variant_t::operator=(__LONG32 lSrc) {
  if(V_VT(this)==VT_I4) V_I4(this) = lSrc;
  else if(V_VT(this)==VT_ERROR) V_ERROR(this) = lSrc;
  else if(V_VT(this)==VT_BOOL) V_BOOL(this) = (lSrc ? VARIANT_TRUE : VARIANT_FALSE);
  else {
    Clear();
    V_VT(this) = VT_I4;
    V_I4(this) = lSrc;
  }
  return *this;
}
inline _variant_t &_variant_t::operator=(float fltSrc) {
  if(V_VT(this)!=VT_R4) {
    Clear();
    V_VT(this) = VT_R4;
  }
  V_R4(this) = fltSrc;
  return *this;
}

inline _variant_t &_variant_t::operator=(double dblSrc)
{
  if(V_VT(this)==VT_R8) {
    V_R8(this) = dblSrc;
  }
  else if(V_VT(this)==VT_DATE) {
    V_DATE(this) = dblSrc;
  }
  else {

    Clear();

    V_VT(this) = VT_R8;
    V_R8(this) = dblSrc;
  }

  return *this;
}

inline _variant_t &_variant_t::operator=(const CY &cySrc)
{
  if(V_VT(this)!=VT_CY) {

    Clear();

    V_VT(this) = VT_CY;
  }

  V_CY(this) = cySrc;

  return *this;
}

inline _variant_t &_variant_t::operator=(const _bstr_t &bstrSrc)
{
  _COM_ASSERT(V_VT(this)!=VT_BSTR || !((BSTR) bstrSrc) || V_BSTR(this)!=(BSTR) bstrSrc);

  Clear();

  V_VT(this) = VT_BSTR;

  if(!bstrSrc) {
    V_BSTR(this) = NULL;
  }
  else {
    BSTR bstr = static_cast<wchar_t *>(bstrSrc);
    V_BSTR(this) = ::SysAllocStringByteLen(reinterpret_cast<char *>(bstr),::SysStringByteLen(bstr));

    if(!(V_BSTR(this))) {
      _com_issue_error(E_OUTOFMEMORY);
    }
  }

  return *this;
}

inline _variant_t &_variant_t::operator=(const wchar_t *pSrc)
{
  _COM_ASSERT(V_VT(this)!=VT_BSTR || !pSrc || V_BSTR(this)!=pSrc);

  Clear();

  V_VT(this) = VT_BSTR;

  if(!pSrc) {
    V_BSTR(this) = NULL;
  }
  else {
    V_BSTR(this) = ::SysAllocString(pSrc);

    if(!(V_BSTR(this))) {
      _com_issue_error(E_OUTOFMEMORY);
    }
  }

  return *this;
}

inline _variant_t &_variant_t::operator=(const char *pSrc)
{
  _COM_ASSERT(V_VT(this)!=(VT_I1 | VT_BYREF) || !pSrc || V_I1REF(this)!=pSrc);

  Clear();

  V_VT(this) = VT_BSTR;
  V_BSTR(this) = _com_util::ConvertStringToBSTR(pSrc);

  return *this;
}

inline _variant_t &_variant_t::operator=(IDispatch *pSrc)
{
  _COM_ASSERT(V_VT(this)!=VT_DISPATCH || pSrc==0 || V_DISPATCH(this)!=pSrc);

  Clear();

  V_VT(this) = VT_DISPATCH;
  V_DISPATCH(this) = pSrc;

  if(V_DISPATCH(this)!=NULL) {

    V_DISPATCH(this)->AddRef();
  }

  return *this;
}

inline _variant_t &_variant_t::operator=(bool boolSrc)
{
  if(V_VT(this)!=VT_BOOL) {

    Clear();

    V_VT(this) = VT_BOOL;
  }

  V_BOOL(this) = (boolSrc ? VARIANT_TRUE : VARIANT_FALSE);

  return *this;
}

inline _variant_t &_variant_t::operator=(IUnknown *pSrc)
{
  _COM_ASSERT(V_VT(this)!=VT_UNKNOWN || !pSrc || V_UNKNOWN(this)!=pSrc);

  Clear();

  V_VT(this) = VT_UNKNOWN;
  V_UNKNOWN(this) = pSrc;

  if(V_UNKNOWN(this)!=NULL) {

    V_UNKNOWN(this)->AddRef();
  }

  return *this;
}

inline _variant_t &_variant_t::operator=(const DECIMAL &decSrc)
{
  if(V_VT(this)!=VT_DECIMAL) {

    Clear();
  }

  V_DECIMAL(this) = decSrc;
  V_VT(this) = VT_DECIMAL;

  return *this;
}

inline _variant_t &_variant_t::operator=(BYTE bSrc)
{
  if(V_VT(this)!=VT_UI1) {

    Clear();

    V_VT(this) = VT_UI1;
  }

  V_UI1(this) = bSrc;

  return *this;
}

inline _variant_t &_variant_t::operator=(char cSrc)
{
  if(V_VT(this)!=VT_I1) {

    Clear();

    V_VT(this) = VT_I1;
  }

  V_I1(this) = cSrc;

  return *this;
}

inline _variant_t &_variant_t::operator=(unsigned short usSrc)
{
  if(V_VT(this)!=VT_UI2) {

    Clear();

    V_VT(this) = VT_UI2;
  }

  V_UI2(this) = usSrc;

  return *this;
}

inline _variant_t &_variant_t::operator=(unsigned __LONG32 ulSrc)
{
  if(V_VT(this)!=VT_UI4) {

    Clear();

    V_VT(this) = VT_UI4;
  }

  V_UI4(this) = ulSrc;

  return *this;
}

#ifndef __CYGWIN__
inline _variant_t &_variant_t::operator=(int iSrc)
{
  if(V_VT(this)!=VT_INT) {

    Clear();

    V_VT(this) = VT_INT;
  }

  V_INT(this) = iSrc;

  return *this;
}

inline _variant_t &_variant_t::operator=(unsigned int uiSrc)
{
  if(V_VT(this)!=VT_UINT) {

    Clear();

    V_VT(this) = VT_UINT;
  }

  V_UINT(this) = uiSrc;

  return *this;
}
#endif

__MINGW_EXTENSION inline _variant_t &_variant_t::operator=(__int64 i8Src) {
  if(V_VT(this)!=VT_I8) {

    Clear();

    V_VT(this) = VT_I8;
  }

  V_I8(this) = i8Src;

  return *this;
}

__MINGW_EXTENSION inline _variant_t &_variant_t::operator=(unsigned __int64 ui8Src) {
  if(V_VT(this)!=VT_UI8) {

    Clear();

    V_VT(this) = VT_UI8;
  }

  V_UI8(this) = ui8Src;

  return *this;
}

inline bool _variant_t::operator==(const VARIANT &varSrc) const throw() {
  return *this==&varSrc;
}

inline bool _variant_t::operator==(const VARIANT *pSrc) const throw()
{
  if(!pSrc) {
    return false;
  }

  if(this==pSrc) {
    return true;
  }

  if(V_VT(this)!=V_VT(pSrc)) {
    return false;
  }

  switch (V_VT(this)) {
case VT_EMPTY:
case VT_NULL:
  return true;

case VT_I2:
  return V_I2(this)==V_I2(pSrc);

case VT_I4:
  return V_I4(this)==V_I4(pSrc);

case VT_R4:
  return V_R4(this)==V_R4(pSrc);

case VT_R8:
  return V_R8(this)==V_R8(pSrc);

case VT_CY:
  return memcmp(&(V_CY(this)),&(V_CY(pSrc)),sizeof(CY))==0;

case VT_DATE:
  return V_DATE(this)==V_DATE(pSrc);

case VT_BSTR:
  return (::SysStringByteLen(V_BSTR(this))==::SysStringByteLen(V_BSTR(pSrc))) &&
    (memcmp(V_BSTR(this),V_BSTR(pSrc),::SysStringByteLen(V_BSTR(this)))==0);

case VT_DISPATCH:
  return V_DISPATCH(this)==V_DISPATCH(pSrc);

case VT_ERROR:
  return V_ERROR(this)==V_ERROR(pSrc);

case VT_BOOL:
  return V_BOOL(this)==V_BOOL(pSrc);

case VT_UNKNOWN:
  return V_UNKNOWN(this)==V_UNKNOWN(pSrc);

case VT_DECIMAL:
  return memcmp(&(V_DECIMAL(this)),&(V_DECIMAL(pSrc)),sizeof(DECIMAL))==0;

case VT_UI1:
  return V_UI1(this)==V_UI1(pSrc);

case VT_I1:
  return V_I1(this)==V_I1(pSrc);

case VT_UI2:
  return V_UI2(this)==V_UI2(pSrc);

case VT_UI4:
  return V_UI4(this)==V_UI4(pSrc);

case VT_INT:
  return V_INT(this)==V_INT(pSrc);

case VT_UINT:
  return V_UINT(this)==V_UINT(pSrc);

case VT_I8:
  return V_I8(this)==V_I8(pSrc);

case VT_UI8:
  return V_UI8(this)==V_UI8(pSrc);

default:
  _com_issue_error(E_INVALIDARG);

  }

  return false;
}

inline bool _variant_t::operator!=(const VARIANT &varSrc) const throw()
{
  return !(*this==&varSrc);
}

inline bool _variant_t::operator!=(const VARIANT *pSrc) const throw()
{
  return !(*this==pSrc);
}

inline void _variant_t::Clear()
{
  _com_util::CheckError(::VariantClear(this));
}

inline void _variant_t::Attach(VARIANT &varSrc)
{

  Clear();

  _COM_MEMCPY_S(this,sizeof(varSrc),&varSrc,sizeof(varSrc));
  V_VT(&varSrc) = VT_EMPTY;
}

inline VARIANT _variant_t::Detach()
{
  VARIANT varResult = *this;
  V_VT(this) = VT_EMPTY;

  return varResult;
}

inline VARIANT &_variant_t::GetVARIANT() throw()
{
  return *(VARIANT*) this;
}

inline VARIANT *_variant_t::GetAddress() {
  Clear();
  return (VARIANT*) this;
}
inline void _variant_t::ChangeType(VARTYPE vartype,const _variant_t *pSrc) {
  if(!pSrc) pSrc = this;
  if((this!=pSrc) || (vartype!=V_VT(this))) {
    _com_util::CheckError(::VariantChangeType(static_cast<VARIANT*>(this),const_cast<VARIANT*>(static_cast<const VARIANT*>(pSrc)),0,vartype));
  }
}
inline void _variant_t::SetString(const char *pSrc) { operator=(pSrc); }
inline _variant_t::~_variant_t() throw() { ::VariantClear(this); }
inline _bstr_t::_bstr_t(const _variant_t &var) : m_Data(NULL) {
  if(V_VT(&var)==VT_BSTR) {
    *this = V_BSTR(&var);
    return;
  }
  _variant_t varDest;
  varDest.ChangeType(VT_BSTR,&var);
  *this = V_BSTR(&varDest);
}
inline _bstr_t &_bstr_t::operator=(const _variant_t &var) {
  if(V_VT(&var)==VT_BSTR) {
    *this = V_BSTR(&var);
    return *this;
  }
  _variant_t varDest;
  varDest.ChangeType(VT_BSTR,&var);
  *this = V_BSTR(&varDest);
  return *this;
}

extern _variant_t vtMissing;

#ifndef _USE_RAW
#define bstr_t _bstr_t
#define variant_t _variant_t
#endif

#pragma pop_macro("new")

/* We use _com_issue_error here, but we only provide its inline version in comdef.h,
 * so we need to make sure that it's included as well. */
#include <comdef.h>

#endif /* __cplusplus */

#endif
