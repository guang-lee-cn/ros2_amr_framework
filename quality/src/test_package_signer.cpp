/// @file test_package_signer.cpp — OTA 签名验证器单测（§8.3-2 恒真桩替换）
/// 正向往返 + 全部 fail-closed 反例：篡改载荷/换公钥/坏 base64/空输入/坏 PEM。
#include "ros2_robot_middleware/domain/ota/package_signer.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

using amr::domain::ota::PackageSigner;
using amr::domain::ota::update_manifest;

class PackageSignerTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    ASSERT_TRUE(PackageSigner::generate_keypair(priv_, pub_));
    other_priv_.clear();
    std::string other_pub;
    ASSERT_TRUE(PackageSigner::generate_keypair(other_priv_, other_pub));
    other_pub_ = other_pub;
  }
  std::string priv_, pub_, other_priv_, other_pub_;
};

TEST_F(PackageSignerTest, Given_ValidSignature_Then_VerifyPasses) {
  const auto m = update_manifest(42);
  const auto sig = PackageSigner::sign(m, priv_);
  ASSERT_FALSE(sig.empty());
  EXPECT_TRUE(PackageSigner::verify(m, sig, pub_));
}

TEST_F(PackageSignerTest, Given_TamperedManifest_Then_Rejected) {
  const auto sig = PackageSigner::sign(update_manifest(42), priv_);
  EXPECT_FALSE(PackageSigner::verify(update_manifest(43), sig, pub_));  // 版本被换
  EXPECT_FALSE(PackageSigner::verify("amr-ota:v42 ", sig, pub_));        // 载荷被加料
}

TEST_F(PackageSignerTest, Given_WrongPublicKey_Then_Rejected) {
  const auto sig = PackageSigner::sign(update_manifest(42), priv_);
  EXPECT_FALSE(PackageSigner::verify(update_manifest(42), sig, other_pub_));
}

TEST_F(PackageSignerTest, Given_TamperedSignature_Then_Rejected) {
  const auto sig = PackageSigner::sign(update_manifest(42), priv_);
  auto bad = sig;
  bad[0] = (bad[0] == 'A') ? 'B' : 'A';  // 翻一个 base64 字符
  EXPECT_FALSE(PackageSigner::verify(update_manifest(42), bad, pub_));
}

TEST_F(PackageSignerTest, Given_MalformedInputs_Then_Rejected) {
  const auto m = update_manifest(42);
  const auto sig = PackageSigner::sign(m, priv_);
  EXPECT_FALSE(PackageSigner::verify(m, "", pub_));            // 空签名
  EXPECT_FALSE(PackageSigner::verify("", sig, pub_));          // 空 manifest
  EXPECT_FALSE(PackageSigner::verify(m, sig, ""));             // 空公钥
  EXPECT_FALSE(PackageSigner::verify(m, "not@base64!", pub_)); // 坏 base64
  EXPECT_FALSE(PackageSigner::verify(m, sig, "-----BEGIN NOT A KEY-----")); // 坏 PEM
  EXPECT_FALSE(PackageSigner::verify(m, "AAAA", pub_));        // 合法 b64 但长度错的签名
}

TEST_F(PackageSignerTest, Given_ManifestFormat_Then_Deterministic) {
  EXPECT_EQ(update_manifest(42), "amr-ota:v42");
  EXPECT_NE(update_manifest(42), update_manifest(420));  // 不同版本不同串
}

}  // namespace
