# Development Note

---

## 2026-06-20 - DB / Redis 연결 구조 구성

### 작업 내용
- MySQL 연결 테스트 완료
- Redis 연결 테스트 완료
- MySQLConnectionPool 싱글톤 구조 작성
- RedisConnection 싱글톤 구조 작성

### 싱글톤 설계 이유
서버에서 DB와 Redis 연결 객체를 여러 곳에서 직접 생성하면 관리가 복잡해질 수 있다고 판단.   
따라서 연결 관리 객체를 싱글톤으로 분리하여 서버 전체에서 하나의 진입점으로 접근하도록 구성

### 다음 작업
- Pub/Sub 구조 설계
- Redis 키 구조 설계
- 조회한 착용 의상 정보를 Redis Hash 구조로 캐싱
- 로그인 시 유저 데이터 DB 조회
- 로비 서버 환경 설정


<br>


## 2026-06-21 - Redis Pub/Sub 적용을 위한 학습 및 설계

### 적용 후보
- 파티 초대 이벤트
- 유저 따라가기 요청
- 파티원 입장/퇴장 알림
- 파티원의 착용 의상 변경 알림

### 설계 방향
Pub/Sub은 서버 간 실시간 이벤트 알림 용도로 사용  

> 단, Pub/Sub 메시지는 저장되지 않기 때문에    
> 유저 상태, 파티 상태, 의상 정보의 원본 데이터는 Redis Hash 또는 DB에 저장하고,   
> Pub/Sub은 변경 사실을 다른 로비 서버에 알려주는 용도로만 사용   

### 다음 작업
- Pub/Sub 구조 설계
- 이벤트 메시지 구조 정의
- 로비 서버별 구독 처리 방식 검토


<br>


## 2026-06-22 - 로그인 서버 IOCP 기반 구조 구축

### 작업 내용   
기존 MMO 서버의 IOCP 구조를 이번 프로젝트 형태에 맞게 이식 및 수정
AcceptEx, WSARecv, WSASend 기본 구조 적용 완료

### 작업 범위      
클라이언트 요청 수신 ~ 로직 처리 직전까지.   
Accept / Send / Recv 처리만 구성한 상태이며 실제 패킷 로직은 미포함  

### 다음 작업   
Pub/Sub 구조 설계
이벤트 메시지 구조 정의
로비 서버별 구독 처리 방식 검토


<br>


## 2026-06-23 - 로드 밸런싱 설계 및 로비 서버 IOCP 구조 구축

### 작업 내용

#### 로그인 서버

- 유저 로그인 인증 완료 후 로비 서버 배정 로직 구현   
- Redis MGET으로 각 로비 서버 인원수 조회 후 최소 인원 서버 선택   
- TTL 만료 키 필터링으로 다운된 로비 서버 자동 배정 제외 처리
- LobbyBalancer 클래스 설계 (GetAvailableServers, SelectServer)


#### 로비 서버

- 기존 MMO 서버의 IOCP 구조 이식 및 수정 완료    
- AcceptEx, WSARecv, WSASend 기본 구조 적용   
- LobbyHeartbeat 클래스 구현   

1. atomic 카운터로 현재 접속자 수 스레드 안전하게 관리     
2. 주기적으로(5초) Redis에 현재 인원수 SET + TTL(15초) 갱신   
3. 정상 종료 시 Redis 키 즉시 삭제 → 무중단 롤링 업데이트 지원   
4. Redis 일시 장애 시 루프 유지, 다음 주기 재시도   


### 설계 결정 이유

인원수 관리를 INCR/DECR가 아닌 덮어쓰기(SET) 방식으로 채택  

비정상 종료 시 누수 카운트 누적 방지   
heartbeat 끊기면 TTL 만료로 해당 서버 자동 배제        
(heartbeat 주기(5초) × 3 = TTL(15초)로 설정)   

일시적 네트워크 불안정 허용, 오탐 방지


> 정상 종료: redis.del(key) 즉시 호출 → 즉각 배정 제외         
> 비정상 종료: TTL 만료로 15초 내 자동 배제 (이중 안전망)   


### 다음 작업

로비 서버 세션 토큰 검증 및 유저 등록 처리   
MySQL 테이블 설계 마무리   
Redis 키 구조 문서화


<br>


## 2026-06-25 - 비밀번호 암호화 학습 및 유틸리티 구현

### 작업 내용

SHA256 + Salt 기반 비밀번호 해시 방식 학습   
PasswordUtils 파일 추가 (OpenSSL 기반)

SHA256Hash() - 문자열 SHA256 해시 후 16진수 문자열 반환   
GenerateSalt() - OpenSSL RAND_bytes로 암호학적 난수 Salt 생성   


### 설계 결정 이유

평문 저장 대신 SHA256 + Salt로 비밀번호 보안 강화      
Salt를 유저마다 다르게 생성해 레인보우 테이블 공격 방어         
bcrypt 대비 구현 단순, 실제 서비스라면 bcrypt/Argon2 적용 필요      

### 다음 작업

LoginCheck DB 조회 구현 (prepared statement + 비밀번호 검증)   
회원가입 시 Salt 생성 및 해시 저장 흐름 구현


<br>


## 2026-06-26 - 로그인 체크 및 회원가입 암호화 적용

### 작업 내용

- LoginCheck 구현

prepared statement로 SQL injection 방지   
DB에서 user_id로 조회 후 저장된 Salt 가져오기   
SHA256(입력 비번 + Salt) == DB 해시값 비교   
성공 시 user_pk 반환 (std::optional<uint32_t>)   


- 회원가입 흐름 구현

GenerateSalt()로 유저별 고유 Salt 생성   
SHA256(비번 + Salt) 해시 후 DB 저장   


- DB 테이블 생성

DB 스키마 확정 및 schema.sql 추가   
user, user_equip_slot, user_inventory, user_currency, friend 테이블 생성


### 다음 작업

ProcessConnect 구현 (로그인 성공 후 유저 데이터 순차 조회)    
GetUserInfo, GetUserCurrency, GetUserCostume DB 조회 함수 구현   


<br>


## 2026-06-27 - 로그인 성공 후 유저 데이터 조회 구현

### 작업 내용

GetUserInfo - user_pk로 유저 정보(레벨/경험치) DB 조회    
GetUserCurrency - 유저 화폐(BP/G-COIN) DB 조회   
GetUserCostume - 현재 착용 코스튬 슬롯별 DB 조회
ProcessConnect 구현   

로그인 성공 시 위 조회들을 순차 실행   
LoadBalancer 연동으로 최소 인원 로비 서버 배정   
응답 패킷(USER_LOGIN_RESPONSE)에 유저 정보 + 코스튬 + 서버 주소 구성   



### 설계 결정 이유

로그인 응답을 단일 패킷으로 통합   
TCP가 분할/조립을 처리하므로 구조체 크기 문제 없음   
클라가 헤더 사이즈 확인 후 그만큼 recv하는 패턴 적용   



### 다음 작업

ProcessConnect 완성 (인벤토리 조회, Redis 세션 저장, JWT 토큰 발급)    
코스튬 데이터 Redis Hash에 캐싱 (user:{pk}:equip_slot)   
로비 서버 세션 토큰 검증 및 유저 등록 처리   
