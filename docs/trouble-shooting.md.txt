# Trouble Shooting

---

## DB


### MySQL Access denied 오류 (2026-06-20)

#### 문제
MySQL 연결 시 'Access denied for user' 오류 발생

#### 원인
C++ 서버에서 사용하는 계정 정보와 MySQL에 설정된 계정 권한이 일치하지 않아서 발생

#### 해결
MySQL 계정 권한과 비밀번호를 확인하고, 서버 연결 설정을 수정


---

## Server