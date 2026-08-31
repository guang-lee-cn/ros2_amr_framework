#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_OTA_PACKAGE_SIGNER_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_OTA_PACKAGE_SIGNER_HPP_

/// @file   package_signer.hpp
/// @brief  OTA 更新包签名验证（§8.3-2：替换恒真桩，ed25519 detached 签名）。
///
/// 信任模型：
///   - 私钥离线持有（构建/交付侧签发），公钥烧录在设备上（agent 参数注入）
///   - 被签对象 = update_manifest(version)（版本的规范串）
///   - 签名 base64 编码随版本一同送达；**缺失/错误/无法解析一律拒绝**
///     （fail-closed——2026-08-25 审计：旧版 /*signature_valid=*/true 恒真）
///
/// 纯 OpenSSL（EVP ed25519），零 ROS2 头文件——domain 红线只禁 ROS。

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace amr {
namespace domain {
namespace ota {

/// 被签名的版本规范串（防跨字段拼接歧义：单一确定格式）
inline std::string update_manifest(int64_t version)
{
  return "amr-ota:v" + std::to_string(version);
}

/// P0-D（三审 2026-08-31）：内容绑定 manifest——签名对象从裸版本号升级为
/// {version, sha256, size} 整体。旧格式只证明「有人授权过这个版本号」，
/// 不证明「进槽的就是那份镜像」——合法签名 + 任意恶意内容可进槽。
inline std::string image_manifest(int64_t version, const std::string &sha256_hex,
                                  uint64_t size_bytes)
{
  return "amr-ota:v" + std::to_string(version) + ";sha256=" + sha256_hex +
         ";size=" + std::to_string(size_bytes);
}

/// SHA-256 摘要（hex 小写）。镜像内容复验的哈希源——OpenSSL EVP，
/// 与签名器同文件（domain 纯度：零 ROS 头）。
inline std::string sha256_hex(const void *data, std::size_t len)
{
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int md_len = 0;
  if (!EVP_Digest(data, len, md, &md_len, EVP_sha256(), nullptr) || md_len == 0) {
    return {};
  }
  static const char kHex[] = "0123456789abcdef";
  std::string out(md_len * 2, '0');
  for (unsigned int i = 0; i < md_len; ++i) {
    out[2 * i]     = kHex[md[i] >> 4];
    out[2 * i + 1] = kHex[md[i] & 0xF];
  }
  return out;
}

namespace detail {

inline std::string b64_encode(const unsigned char *data, std::size_t len)
{
  std::string out(4 * ((len + 2) / 3) + 1, '\0');
  int n = EVP_EncodeBlock(reinterpret_cast<unsigned char *>(out.data()), data,
                          static_cast<int>(len));
  out.resize(n > 0 ? static_cast<std::size_t>(n) : 0);
  return out;
}

inline bool b64_decode(const std::string &in, std::vector<unsigned char> &out)
{
  if (in.empty() || in.size() % 4 != 0) return false;
  out.resize(3 * in.size() / 4 + 1);
  int n = EVP_DecodeBlock(out.data(),
                          reinterpret_cast<const unsigned char *>(in.data()),
                          static_cast<int>(in.size()));
  if (n <= 0) return false;
  // EVP_DecodeBlock 不吸收 '=' 填充计数，手动裁掉（C++17：无 ends_with）
  std::size_t pad = 0;
  if (in.size() >= 2 && in.compare(in.size() - 2, 2, "==") == 0) pad = 2;
  else if (in.back() == '=') pad = 1;
  if (static_cast<std::size_t>(n) < pad) return false;
  out.resize(static_cast<std::size_t>(n) - pad);
  return true;
}

struct EvpKeyDeleter { void operator()(EVP_PKEY *p) const { EVP_PKEY_free(p); } };
struct BioDeleter { void operator()(BIO *b) const { BIO_free(b); } };

inline EVP_PKEY *load_public_pem(const std::string &pem)
{
  std::unique_ptr<BIO, BioDeleter> bio(BIO_new_mem_buf(pem.data(), -1));
  return bio ? PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr) : nullptr;
}

inline EVP_PKEY *load_private_pem(const std::string &pem)
{
  std::unique_ptr<BIO, BioDeleter> bio(BIO_new_mem_buf(pem.data(), -1));
  return bio ? PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr) : nullptr;
}

}  // namespace detail

class PackageSigner {
public:
    /// 生成 ed25519 密钥对（测试与交付侧演示用；设备侧只烧公钥）。
    /// 返回 false = OpenSSL 失败（如熵不足）。
    static bool generate_keypair(std::string &private_pem, std::string &public_pem)
    {
      EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
      if (!ctx) return false;
      std::unique_ptr<EVP_PKEY_CTX, void (*)(EVP_PKEY_CTX *)> ctx_guard(
          ctx, [](EVP_PKEY_CTX *c) { EVP_PKEY_CTX_free(c); });
      if (EVP_PKEY_keygen_init(ctx) <= 0) return false;
      EVP_PKEY *raw = nullptr;
      if (EVP_PKEY_keygen(ctx, &raw) <= 0) return false;
      std::unique_ptr<EVP_PKEY, detail::EvpKeyDeleter> key(raw);

      private_pem = pem_to_string([&](BIO *b) {
        return PEM_write_bio_PrivateKey(b, key.get(), nullptr, nullptr, 0, nullptr, nullptr);
      });
      public_pem = pem_to_string([&](BIO *b) {
        return PEM_write_bio_PUBKEY(b, key.get());
      });
      return !private_pem.empty() && !public_pem.empty();
    }

    /// 签名（交付侧/测试）：manifest → base64(ed25519 签名)。失败返回空串。
    static std::string sign(const std::string &manifest, const std::string &private_pem)
    {
      std::unique_ptr<EVP_PKEY, detail::EvpKeyDeleter> key(detail::load_private_pem(private_pem));
      if (!key) return {};
      EVP_MD_CTX *ctx = EVP_MD_CTX_new();
      if (!ctx) return {};
      std::unique_ptr<EVP_MD_CTX, void (*)(EVP_MD_CTX *)> guard(
          ctx, [](EVP_MD_CTX *c) { EVP_MD_CTX_free(c); });
      std::size_t siglen = 0;
      if (EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, key.get()) <= 0) return {};
      if (EVP_DigestSign(ctx, nullptr, &siglen,
                         reinterpret_cast<const unsigned char *>(manifest.data()),
                         manifest.size()) <= 0) return {};
      std::vector<unsigned char> sig(siglen);
      if (EVP_DigestSign(ctx, sig.data(), &siglen,
                         reinterpret_cast<const unsigned char *>(manifest.data()),
                         manifest.size()) <= 0) return {};
      return detail::b64_encode(sig.data(), siglen);
    }

    /// 验签（设备侧）：base64 签名 + 公钥 PEM。任何失败 = false（fail-closed）。
    static bool verify(const std::string &manifest, const std::string &signature_b64,
                       const std::string &public_pem)
    {
      if (manifest.empty() || signature_b64.empty() || public_pem.empty()) return false;
      std::vector<unsigned char> sig;
      if (!detail::b64_decode(signature_b64, sig) || sig.empty()) return false;
      std::unique_ptr<EVP_PKEY, detail::EvpKeyDeleter> key(detail::load_public_pem(public_pem));
      if (!key) return false;
      EVP_MD_CTX *ctx = EVP_MD_CTX_new();
      if (!ctx) return false;
      std::unique_ptr<EVP_MD_CTX, void (*)(EVP_MD_CTX *)> guard(
          ctx, [](EVP_MD_CTX *c) { EVP_MD_CTX_free(c); });
      if (EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, key.get()) <= 0) return false;
      return EVP_DigestVerify(ctx, sig.data(), sig.size(),
                              reinterpret_cast<const unsigned char *>(manifest.data()),
                              manifest.size()) == 1;
    }

private:
    template <typename WriteFn>
    static std::string pem_to_string(WriteFn write_fn)
    {
      std::unique_ptr<BIO, detail::BioDeleter> bio(BIO_new(BIO_s_mem()));
      if (!bio || write_fn(bio.get()) <= 0) return {};
      char *data = nullptr;
      long len = BIO_get_mem_data(bio.get(), &data);
      if (len <= 0 || !data) return {};
      return std::string(data, static_cast<std::size_t>(len));
    }
};

}  // namespace ota
}  // namespace domain
}  // namespace amr

#endif  // ROS2_ROBOT_MIDDLEWARE_DOMAIN_OTA_PACKAGE_SIGNER_HPP_
