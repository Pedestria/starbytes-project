# Starbpkg Package Catalog (Re-evaluated, De-duplicated)

Date: February 27, 2026  
Scope: External package repository targets for `starbpkg` (not built-in `stdlib` modules)

## What Changed

This document replaces the prior synthetic submodule-style catalog with a researched list of real, commonly used upstream libraries.

Rules applied:
- Every entry is unique by `(ecosystem, package_id)`.
- No generated submodule placeholders.
- Repeats are avoided; related multi-package families are only listed when the ecosystem treats them as separate widely used packages (for example, `grpc-*`, `symfony/*`, `Microsoft.EntityFrameworkCore.*`).

## Research Basis

Primary ecosystem/popularity references (checked February 2026):
- Top PyPI package dataset and changelog: <https://hugovk.github.io/top-pypi-packages/>.
- PyPI popularity snapshot: <https://libraries.io/pypi>.
- npm popularity snapshot: <https://libraries.io/npm>.
- npm high-impact package definition/data source: <https://www.npmjs.com/package/npm-high-impact>.
- Cargo popularity snapshot: <https://libraries.io/cargo>.
- Go popularity snapshot: <https://libraries.io/go>.
- Maven popularity snapshot: <https://libraries.io/maven>.
- NuGet popularity snapshot: <https://libraries.io/nuget>.
- Packagist popularity snapshot: <https://libraries.io/packagist>.
- RubyGems ecosystem download stats: <https://rubygems.org/stats>.

## Catalog Size

- Total entries: **300** unique package targets.
- Ecosystems covered: npm, PyPI, Cargo, Go, Maven, NuGet, Packagist, RubyGems.

---

## 1) npm (50)

- [npm] react
- [npm] react-dom
- [npm] next
- [npm] vue
- [npm] @angular/core
- [npm] svelte
- [npm] express
- [npm] fastify
- [npm] koa
- [npm] @nestjs/core
- [npm] axios
- [npm] got
- [npm] node-fetch
- [npm] graphql
- [npm] @apollo/server
- [npm] prisma
- [npm] mongoose
- [npm] pg
- [npm] mysql2
- [npm] redis
- [npm] socket.io
- [npm] jsonwebtoken
- [npm] bcrypt
- [npm] dotenv
- [npm] commander
- [npm] yargs
- [npm] chalk
- [npm] ora
- [npm] inquirer
- [npm] zod
- [npm] joi
- [npm] ajv
- [npm] lodash
- [npm] ramda
- [npm] rxjs
- [npm] dayjs
- [npm] date-fns
- [npm] moment
- [npm] uuid
- [npm] nanoid
- [npm] winston
- [npm] pino
- [npm] jest
- [npm] mocha
- [npm] chai
- [npm] vitest
- [npm] eslint
- [npm] prettier
- [npm] typescript
- [npm] webpack

## 2) PyPI (50)

- [pypi] numpy
- [pypi] pandas
- [pypi] scipy
- [pypi] scikit-learn
- [pypi] matplotlib
- [pypi] seaborn
- [pypi] plotly
- [pypi] requests
- [pypi] httpx
- [pypi] urllib3
- [pypi] pydantic
- [pypi] fastapi
- [pypi] django
- [pypi] flask
- [pypi] sqlalchemy
- [pypi] alembic
- [pypi] psycopg2-binary
- [pypi] redis
- [pypi] celery
- [pypi] beautifulsoup4
- [pypi] lxml
- [pypi] scrapy
- [pypi] pillow
- [pypi] opencv-python
- [pypi] pytest
- [pypi] hypothesis
- [pypi] mypy
- [pypi] black
- [pypi] ruff
- [pypi] isort
- [pypi] jinja2
- [pypi] click
- [pypi] typer
- [pypi] rich
- [pypi] loguru
- [pypi] tenacity
- [pypi] boto3
- [pypi] google-cloud-storage
- [pypi] grpcio
- [pypi] protobuf
- [pypi] pyyaml
- [pypi] tomli
- [pypi] orjson
- [pypi] uvicorn
- [pypi] gunicorn
- [pypi] aiohttp
- [pypi] websockets
- [pypi] cryptography
- [pypi] pyjwt
- [pypi] shapely

## 3) Cargo / crates.io (40)

- [cargo] serde
- [cargo] serde_json
- [cargo] tokio
- [cargo] clap
- [cargo] anyhow
- [cargo] thiserror
- [cargo] reqwest
- [cargo] hyper
- [cargo] axum
- [cargo] actix-web
- [cargo] tonic
- [cargo] prost
- [cargo] tracing
- [cargo] tracing-subscriber
- [cargo] log
- [cargo] env_logger
- [cargo] regex
- [cargo] lazy_static
- [cargo] once_cell
- [cargo] chrono
- [cargo] uuid
- [cargo] rand
- [cargo] futures
- [cargo] bytes
- [cargo] parking_lot
- [cargo] rayon
- [cargo] sqlx
- [cargo] diesel
- [cargo] rusqlite
- [cargo] sea-orm
- [cargo] polars
- [cargo] ndarray
- [cargo] nalgebra
- [cargo] image
- [cargo] clap_derive
- [cargo] async-trait
- [cargo] itertools
- [cargo] criterion
- [cargo] serde_yaml
- [cargo] toml

## 4) Go Modules (35)

- [go] github.com/gin-gonic/gin
- [go] github.com/labstack/echo/v4
- [go] github.com/gofiber/fiber/v2
- [go] github.com/go-chi/chi/v5
- [go] google.golang.org/grpc
- [go] github.com/grpc-ecosystem/grpc-gateway/v2
- [go] go.uber.org/zap
- [go] github.com/rs/zerolog
- [go] github.com/sirupsen/logrus
- [go] github.com/spf13/cobra
- [go] github.com/spf13/viper
- [go] github.com/spf13/pflag
- [go] github.com/stretchr/testify
- [go] github.com/google/go-cmp/cmp
- [go] github.com/prometheus/client_golang
- [go] go.opentelemetry.io/otel
- [go] gorm.io/gorm
- [go] github.com/jmoiron/sqlx
- [go] github.com/lib/pq
- [go] github.com/go-sql-driver/mysql
- [go] github.com/redis/go-redis/v9
- [go] go.mongodb.org/mongo-driver
- [go] github.com/gorilla/websocket
- [go] github.com/golang-jwt/jwt/v5
- [go] github.com/go-playground/validator/v10
- [go] github.com/google/uuid
- [go] golang.org/x/sync/errgroup
- [go] golang.org/x/crypto/bcrypt
- [go] github.com/joho/godotenv
- [go] github.com/segmentio/kafka-go
- [go] github.com/nats-io/nats.go
- [go] github.com/aws/aws-sdk-go-v2
- [go] github.com/Azure/azure-sdk-for-go/sdk/azcore
- [go] cloud.google.com/go/storage
- [go] google.golang.org/protobuf

## 5) Maven (35)

- [maven] com.google.guava:guava
- [maven] org.apache.commons:commons-lang3
- [maven] org.apache.commons:commons-collections4
- [maven] commons-io:commons-io
- [maven] org.slf4j:slf4j-api
- [maven] ch.qos.logback:logback-classic
- [maven] org.springframework:spring-core
- [maven] org.springframework:spring-context
- [maven] org.springframework.boot:spring-boot-starter-web
- [maven] org.springframework.boot:spring-boot-starter-data-jpa
- [maven] com.fasterxml.jackson.core:jackson-databind
- [maven] com.fasterxml.jackson.core:jackson-core
- [maven] com.fasterxml.jackson.core:jackson-annotations
- [maven] org.junit.jupiter:junit-jupiter
- [maven] org.mockito:mockito-core
- [maven] org.assertj:assertj-core
- [maven] org.projectlombok:lombok
- [maven] org.hibernate.orm:hibernate-core
- [maven] org.jetbrains.kotlin:kotlin-stdlib
- [maven] org.jetbrains.kotlinx:kotlinx-coroutines-core
- [maven] org.jetbrains.kotlinx:kotlinx-serialization-json
- [maven] io.projectreactor:reactor-core
- [maven] io.netty:netty-all
- [maven] org.apache.httpcomponents.client5:httpclient5
- [maven] com.squareup.okhttp3:okhttp
- [maven] com.squareup.retrofit2:retrofit
- [maven] com.squareup.moshi:moshi
- [maven] org.flywaydb:flyway-core
- [maven] org.liquibase:liquibase-core
- [maven] org.apache.kafka:kafka-clients
- [maven] io.grpc:grpc-netty
- [maven] io.grpc:grpc-protobuf
- [maven] io.grpc:grpc-stub
- [maven] org.postgresql:postgresql
- [maven] mysql:mysql-connector-java

## 6) NuGet (35)

- [nuget] Newtonsoft.Json
- [nuget] System.Text.Json
- [nuget] Serilog
- [nuget] Serilog.AspNetCore
- [nuget] NLog
- [nuget] Dapper
- [nuget] Microsoft.EntityFrameworkCore
- [nuget] Microsoft.EntityFrameworkCore.SqlServer
- [nuget] Microsoft.EntityFrameworkCore.Tools
- [nuget] AutoMapper
- [nuget] FluentValidation
- [nuget] MediatR
- [nuget] Polly
- [nuget] Refit
- [nuget] RestSharp
- [nuget] Swashbuckle.AspNetCore
- [nuget] Microsoft.AspNetCore.Authentication.JwtBearer
- [nuget] Microsoft.IdentityModel.Tokens
- [nuget] xunit
- [nuget] xunit.runner.visualstudio
- [nuget] NUnit
- [nuget] Moq
- [nuget] Bogus
- [nuget] MassTransit
- [nuget] Quartz
- [nuget] StackExchange.Redis
- [nuget] Npgsql
- [nuget] MySqlConnector
- [nuget] MongoDB.Driver
- [nuget] Grpc.Net.Client
- [nuget] Google.Protobuf
- [nuget] Azure.Storage.Blobs
- [nuget] AWSSDK.S3
- [nuget] CsvHelper
- [nuget] BenchmarkDotNet

## 7) Packagist (30)

- [packagist] laravel/framework
- [packagist] symfony/framework-bundle
- [packagist] symfony/console
- [packagist] symfony/http-foundation
- [packagist] symfony/routing
- [packagist] guzzlehttp/guzzle
- [packagist] monolog/monolog
- [packagist] phpunit/phpunit
- [packagist] phpstan/phpstan
- [packagist] friendsofphp/php-cs-fixer
- [packagist] doctrine/orm
- [packagist] doctrine/dbal
- [packagist] ramsey/uuid
- [packagist] predis/predis
- [packagist] firebase/php-jwt
- [packagist] league/flysystem
- [packagist] twig/twig
- [packagist] nesbot/carbon
- [packagist] vlucas/phpdotenv
- [packagist] spatie/laravel-permission
- [packagist] spatie/laravel-data
- [packagist] livewire/livewire
- [packagist] inertiajs/inertia-laravel
- [packagist] php-amqplib/php-amqplib
- [packagist] elastic/elasticsearch
- [packagist] aws/aws-sdk-php
- [packagist] symfony/mailer
- [packagist] symfony/validator
- [packagist] league/oauth2-client
- [packagist] endroid/qr-code

## 8) RubyGems (25)

- [rubygems] rails
- [rubygems] rack
- [rubygems] rake
- [rubygems] bundler
- [rubygems] rspec
- [rubygems] minitest
- [rubygems] rubocop
- [rubygems] sinatra
- [rubygems] puma
- [rubygems] sidekiq
- [rubygems] devise
- [rubygems] nokogiri
- [rubygems] pg
- [rubygems] redis
- [rubygems] faraday
- [rubygems] httparty
- [rubygems] jwt
- [rubygems] dry-validation
- [rubygems] sequel
- [rubygems] hanami
- [rubygems] grape
- [rubygems] capybara
- [rubygems] factory_bot
- [rubygems] simplecov
- [rubygems] dotenv

---

## starbpkg Integration Notes

- Treat each entry above as an upstream package source candidate in a separate package repository.
- Suggested manifest fields per package in the external index:
  - `ecosystem`
  - `package_id`
  - `version`
  - `source_url`
  - `license`
  - `checksum`
  - `dependencies`
  - `build_adapter`
  - `platforms`
- Keep compatibility metadata explicit (`starbytes_min`, `starbytes_max`) to avoid ecosystem drift.

## Validation Checklist

- [x] A few hundred entries.
- [x] No synthetic submodule placeholders.
- [x] De-duplicated by `(ecosystem, package_id)`.
- [x] Derived from commonly used ecosystem libraries and package-index popularity references.
