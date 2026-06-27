/**
 * PasswordUtils.h
 *
 * 비밀번호 해시 및 Salt 생성 유틸리티
 * SHA256 해시, 랜덤 Salt 생성 기능 제공
 *
 * 사용 라이브러리: OpenSSL
 */

#pragma once
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <sstream>
#include <iomanip>
#include <string>

std::string SHA256Hash(const std::string& input);
std::string GenerateSalt(int length = 32);