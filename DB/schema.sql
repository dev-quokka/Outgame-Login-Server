-- OutGameServer Database Schema
-- 2026-06-23

-- 유저 정보 테이블
CREATE TABLE user (
    user_pk       INT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id       VARCHAR(20) NOT NULL UNIQUE,
    user_password VARCHAR(60) NOT NULL,
    user_level    INT UNSIGNED NOT NULL DEFAULT 1,
    user_exp      INT UNSIGNED NOT NULL DEFAULT 0,
    created_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,   -- 가입 시각 (첫 Insert때만 입력)
    last_login_at TIMESTAMP NULL DEFAULT NULL,           -- 마지막 접속, 로그인마다 갱신
    PRIMARY KEY (user_pk)
);
	
-- 유저 화폐 테이블
CREATE TABLE user_currency (
    user_pk  INT UNSIGNED NOT NULL,
    bp       INT UNSIGNED NOT NULL DEFAULT 0,   -- 무료 화폐
    gcoin    INT UNSIGNED NOT NULL DEFAULT 0,   -- 유료 화폐
    PRIMARY KEY (user_pk),
    FOREIGN KEY (user_pk) REFERENCES user(user_pk) ON DELETE CASCADE
);

-- 유저별 현재 착용 코스튬
CREATE TABLE user_equip_slot (
    user_pk   INT UNSIGNED NOT NULL,
    slot_type TINYINT UNSIGNED NOT NULL,  -- 1=head, 2=body, 3=legs, 4=feet
    item_code INT UNSIGNED NOT NULL,
    PRIMARY KEY (user_pk, slot_type),
    FOREIGN KEY (user_pk) REFERENCES user(user_pk) ON DELETE CASCADE
);

-- 유저별 보유 아이템 인벤토리
CREATE TABLE user_inventory (
    user_pk   INT UNSIGNED NOT NULL,
    item_code INT UNSIGNED NOT NULL,
    item_type TINYINT UNSIGNED NOT NULL,  -- 1=코스튬, 2=총스킨, 3=재료
    quantity  INT UNSIGNED NOT NULL DEFAULT 1,
    PRIMARY KEY (user_pk, item_code),
    FOREIGN KEY (user_pk) REFERENCES user(user_pk) ON DELETE CASCADE
);

-- 유저별 친구 테이블 (양방향 저장)
CREATE TABLE friend (
    user_pk    INT UNSIGNED NOT NULL,
    friend_pk  INT UNSIGNED NOT NULL,
    status     TINYINT UNSIGNED NOT NULL DEFAULT 0,  -- 0=요청중, 1=친구
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,  -- 친구가 된 시각 (첫 Insert때만 입력)
    PRIMARY KEY (user_pk, friend_id), --  복합pk
    FOREIGN KEY (user_pk)   REFERENCES user(user_pk) ON DELETE CASCADE,
    FOREIGN KEY (friend_pk) REFERENCES user(user_pk) ON DELETE CASCADE
);

