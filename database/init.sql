CREATE DATABASE IF NOT EXISTS oj_system DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE oj_system;

CREATE TABLE IF NOT EXISTS problems (
  id         INT PRIMARY KEY AUTO_INCREMENT,
  title      VARCHAR(255) NOT NULL,
  difficulty ENUM('Easy','Medium','Hard') NOT NULL,
  content    TEXT NOT NULL,
  template   TEXT,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS test_cases (
  id         INT PRIMARY KEY AUTO_INCREMENT,
  problem_id INT NOT NULL,
  input      TEXT NOT NULL,
  expected   TEXT NOT NULL,
  position   INT NOT NULL DEFAULT 0,
  FOREIGN KEY (problem_id) REFERENCES problems(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS users (
  id       INT PRIMARY KEY AUTO_INCREMENT,
  username VARCHAR(64) UNIQUE NOT NULL,
  password VARCHAR(128) NOT NULL,
  role     ENUM('user','admin') DEFAULT 'user',
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);