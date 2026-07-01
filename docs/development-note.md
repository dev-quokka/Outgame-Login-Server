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


<br>


## 2026-06-28 - 로그인 프로세스 완성 및 친구 목록 구현

### 작업 내용

- 로그인 프로세스 완성

ProcessConnect 구조 개선

bool 반환 + out 매개변수(inventoryRes, friendRes, serverType) 구조로 변경   
모든 조회 성공 후 한꺼번에 send하는 방식으로 불필요한 트래픽 방지    
성공 시 loginRes + inventoryRes + friendRes 순차 전송    
실패 시 loginRes(isSuccess=false)만 전송  


- 인벤토리 조회 구현

GetUserInventory - user_inventory 테이블 조회   
USER_INVENTORY_PACKET 구조체 추가 (InventoryItem 배열)   
InventoryItem - itemCode, itemType, quantity   


- 친구 목록 조회 구현

GetUserFriendsDB - DB 조회용 내부 구조체 (FriendInfoDB, pk 포함)    
friend + user 테이블 JOIN으로 friendPk + friendId + status 조회


BuildFriendList - DB 결과를 패킷용으로 변환    
친구인 것들만 Redis pipeline으로 한 번에 접속 상태 조회 (N번 -> 1번 왕복)   
pk 제외한 FriendInfo(friendId + friendStatus + onlineStatus)로 변환   


- 로그인 실패 코드 추가

LoginFailCode enum 추가 (WrongPassword, ServerError, NoServer)   
USER_LOGIN_RESPONSE에 failCode 필드 추가   
각 실패 지점에서 원인 코드 설정   


- JWT 토큰 구조 생성

userId + token을 Redis에 저장 (jwtcheck:{userId})   
pk 클라 노출 없이 userId 기반으로 토큰 검증    
1회용 토큰 - 검증 후 즉시 삭제   



### 다음 작업

로비 서버 JWT 토큰 검증 및 유저 등록 처리
친구 접속/로그아웃 pub/sub 알림 구현
로그인 서버 Disconnect 처리


<br>


## 2026-06-29 - 로비 서버 Redis pub/sub 구독 구조 구축

### 작업 내용

- 로비 서버

LobbyRedisSubscriber 클래스 추가

sub.consume() blocking 루프 생성
HandleLobbyEvent type 숫자 파싱 후 switch 분기
HandleFriendOnline/Offline/Accepted/Removed/CostumeChange 뼈대 구현
ParseUintField 헬퍼 - JSON 메시지에서 정수 필드 추출


LobbyEventType enum 추가   

FriendOnline(1), FriendOffline(2), CostumeChange(3), FriendAccepted(6), FriendRemoved(7)



- 로그인 서버

pub/sub 채널 구조 전환

{서버명}:events 타겟 채널별 publish


PublishToUsers 헬퍼 구현

타겟 pk 목록을 pipeline으로 서버 위치 일괄 조회      
온라인 유저만 필터링, 서버별 중복 없이 publish    
개별 예외 처리로 일부 조회 실패해도 나머지는 계속 처리   


NotifyFriendOnline 수정 - PublishToUsers 활용   
NotifyFriendOffline 구현 - Disconnect 시 DB 조회 후 친구들에게 알림   
ProcessFriendAccept/Remove pub/sub 부분 서버별 채널로 수정   


### 다음 작업

Handle 함수별 내부 로직 구현 (세션 순회 + 패킷 전송)


<br>


## 2026-06-30 - Handle 함수 내부 로직 및 ConnUsersManager 확장

### 작업 내용

- 로비 서버

ConnUsersManager에 pk - objNum 역방향 매핑 추가


RedisManager에 로컬 유저 알림 함수 추가   

NotifyFriendStatusToLocalUsers - ForEachUser 순회하며 친구 캐시 확인 후 패킷 전송   
NotifyCostumeChangeToLocalUsers - 파티원 대상 코스튬 변경 알림 (개발중)


HandleFriendOffline/Online 내부 로직 구현

RedisManager::NotifyFriendStatusToLocalUsers 호출로 단순화   
LobbyRedisSubscriber는 처리만하고 실제 send는 RedisManager에서 사용



설계 결정 이유

LobbyRedisSubscriber가 직접 send하지 않는 이유

유저에게 패킷 보내는 책임은 이미 RedisManager가 갖고 있는 역할   
LobbyRedisSubscriber가 ConnUsersManager까지 알게 되면 의존성이 늘어남   
역할 분리: Subscriber는 메시지 해석만, RedisManager는 행동만


pk-objNum 매핑에 TBB 대신 mutex 선택

tbb::concurrent_hash_map은 버킷 단위 락으로 고경합 환경에서 유리   
이 맵은 로그인/로그아웃 시에만 쓰기 발생하는 저빈도 자료구조   
락 경합이 사실상 없어 단순한 mutex로 충분, 복잡도 증가 대비 이득 없다고 판단
추후 관리 유저수가 많아지면 테스트를 통해 도입 여부 판단



### 다음 작업

파티 시스템 Redis 구조 설계 및 구현   
NotifyCostumeChangeToLocalUsers 완성   
HandleFriendAccepted/Removed 내부 로직 구현   