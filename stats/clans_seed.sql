-- Carga inicial de clanes (hoja ClanTags + =SL= y >6S<). Se aplica UNA sola vez:
-- solo inserta si la tabla clans esta vacia. No va en schema.sql porque ese archivo
-- se reaplica seguido y resucitaria los clanes borrados.
--   psql -U dday -d dday -v ON_ERROR_STOP=1 < stats/clans_seed.sql

BEGIN;

DO $seed$
DECLARE
    v_id integer;
BEGIN
    IF EXISTS (SELECT 1 FROM clans) THEN
        RAISE NOTICE 'clans ya tiene datos';
        RETURN;
    END IF;

    -- 1
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('Manicomio de Retards', 'Mimo', 16, 'Tebessa', 'Mimo', 'Que se sentira perder?', 'MR-', 'https://drive.google.com/uc?id=1GN8hlcfjd0CcbvSJQEAaTh3YpOwzn8bB', false, 10, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '[MR]', true);

    -- 2
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('Easy Company', 'Krusty', 11, 'Cualquiera menos Tebessa', 'Patox', 'Rodman Chupalo', 'Nadie es digno', 'https://drive.google.com/uc?id=1W28ICQYQqJUapZt7Avwrf2EGqfa4hVKE', false, 20, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '+=EC=+', true);

    -- 3
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('Reapers Team', 'Hans', 13, 'Dday2', 'Xshot', 'Me lo dijo xian', 'TOT+', 'https://drive.google.com/uc?id=1nf0Y_JAZPNMjOFkfrK-VwVGntaAn2Mlo', false, 30, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '^RT+', true);

    -- 4
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('Animal Instinct Squad', 'Phoenix', 12, 'Dust', 'Cubiak', 'No se tiren como patitos!', 'EC (Hoy)', 'https://drive.google.com/uc?id=1LM2cMWKCSGUXi4cTwo7z8u43P8R3ZWIM', false, 40, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '+AiS->', true);

    -- 5
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('Klan del Kaoz', 'Pacha', 12, 'KF', 'Jaguar', 'Ahhh pacha ql', 'AIS', 'https://drive.google.com/uc?id=1rV6vBjEkiKXnQyueXYp-WYtGl55h8L20', false, 50, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '=K2=', true);

    -- 6
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('Clan Chile', 'Chama', 18, 'Townwar', 'Flema', 'dale dale metele conteíto tkm', 'Lete', 'https://drive.google.com/uc?id=1b4TyjZsb9sqWQ_aH1ubmLx56fYd0gakH', false, 60, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '>ChL<', true);

    -- 7
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('Concejo de Clanes', 'Wii', 6, 'Nuenen', 'CHELO', 'de ahi lo vemos', 'AP', 'https://drive.google.com/uc?id=1gwA0RskQux_n7KxhIywzGLPXJ4p3fvvs', false, 70, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, 'CdC+', true);

    -- 8
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('Tactical Elite xperts', 'Expert', 12, 'Dust', 'Jedi!', 'Khriz y Angel - Showtime', 'Todos', '', false, 80, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '>TeX+', true);

    -- 9
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('Last State of Delirium', 'Ocioso', 3, 'Invade1', 'Voladex', 'me dejay loco', 'Alcapone', 'https://drive.google.com/uc?id=18Cs3zLqifAFWzouxD-NxW7afDhc7xxVK', false, 90, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '[LSD]', true);

    -- 10
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('Pinochet vive', 'Mimo', 4, 'Dust', 'Tillo', 'Mimo pasa la pipa', 'Mimo', 'https://drive.google.com/uc?id=1sysgrUVwfvpiVMavLxJClVgtSSndfJ6n', false, 100, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, 'pV+', true);

    -- 11
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('Fire Eggs', 'Alcapone', 4, '', 'Neoskin', '', 'Expert', 'https://drive.google.com/uc?id=1zCtEPGtlXegqpUTIrFQl3kx0R6Mpz5kx', false, 110, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '+FE+', true);

    -- 12
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('Blood, Bullets & Headshots', 'Pacha', 4, '', '', '', '', 'https://drive.google.com/uc?id=1izJGkCVt9ojSQq5mAOn_cItRqWZKai0b', false, 120, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '-=BBH>', true);

    -- 13
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('Unreal', 'Rodex', 4, '', 'Fics', '', '', 'https://drive.google.com/uc?id=1chcBGqe5fdY0SviUHaMfEaq72pWtdyVf', false, 130, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, 'UNR34L+', true);

    -- 14
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('La tormenta perfecta', 'Damar', 4, '', 'Slash', '', '', 'https://drive.google.com/uc?id=1g_-VYSCHCEDY0vanZWDxct5p4Dp6wrV7', false, 140, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '[LTP]', true);

    -- 15
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('Dale color con la lai', 'Element', 4, '', '', '', '', 'https://drive.google.com/uc?id=1WT1vtDe72iOf4QlJNmQKhcfHiE0f4thG', false, 150, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '[DLL]', true);

    -- 16
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('CiberReptilesPunks', 'Neonblack', 4, '', '', '', '', 'https://drive.google.com/uc?id=14nr5pnTPR9JMyzoI3c1IX3NuvELj3LZe', false, 160, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, ']CRP[', true);

    -- 17
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('WallRide 4', 'Phoenix', 4, '', '', '', '', 'https://drive.google.com/uc?id=1b2b7_hCQpMXfGrQdQ8AFjoh7Z_8AbPsY', false, 170, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '[4WR]', true);

    -- 18
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('Inkebrantables Xtreme', 'Vegeta', 4, '', '', '', '', 'https://drive.google.com/uc?id=1boblYz54WMu1tPjdsR6qn22qvoZ92r5o', false, 180, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '=IKX+', true);

    -- 19
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('Clan No Inscritos', 'Todos', 99, '', '', '', '', '', true, 190, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '>cNi=', true);

    -- 20
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('Troop of Tirany', 'Jericho', 30, 'Dday2', 'Maurox', 'Tanta la rico', '-', '', false, 200, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, 'TOT+', true);

    -- 21
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('', '', NULL, '', '', '', '', '', false, 210, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '[T1]', true);

    -- 22
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('', '', NULL, '', '', '', '', '', false, 220, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, 'TEPT+', true);

    -- 23
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('', '', NULL, '', '', '', '', '', false, 230, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '=UnLPqeTThL', true);

    -- 24
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('', '', NULL, '', '', '', '', '', false, 240, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, 'HAK+', true);

    -- 25
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('', '', NULL, '', '', '', '', '', false, 250, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '<T.T>', true);

    -- 26
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('', '', NULL, '', '', '', '', '', false, 260, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, 'TK[?]', true);

    -- 27
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('', '', NULL, '', '', '', '', '', false, 270, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '[HOT]', true);

    -- 28
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('', '', NULL, '', '', '', '', '', false, 280, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, 'LF+', true);

    -- 29
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('', '', NULL, '', '', '', '', '', false, 290, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '7up', true);

    -- 30
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('', '', NULL, '', '', '', '', '', false, 300, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, 'BhL', true);

    -- 31
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('', '', NULL, '', '', '', '', '', false, 310, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, 'WR', true);

    -- 32
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('', '', NULL, '', '', '', '', '', false, 320, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, 'YF', true);

    -- 33
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('', '', NULL, '', '', '', '', '', false, 330, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, 'HdP', true);

    -- 34
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('', '', NULL, '', '', '', '', '', false, 340, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, 'eNp', true);

    -- 35
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('', '', NULL, '', '', '', '', '', false, 350, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, 'TDB', true);

    -- 36
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('', '', NULL, '', '', '', '', '', false, 360, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, 'LE4', true);

    -- 37
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('', '', NULL, '', '', '', '', '', false, 370, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '=SL=', true);

    -- 38
    INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player, phrase, rival, logo_url, unregistered, sort_order, updated_by)
    VALUES ('', '', NULL, '', '', '', '', '', false, 380, 'carga inicial')
    RETURNING id INTO v_id;
    INSERT INTO clan_tags (clan_id, tag, is_primary) VALUES (v_id, '>6S<', true);
END
$seed$;

COMMIT;
