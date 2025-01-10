FROM gcc:14.2 AS game_asio_builder 
LABEL description="For build and debug game_asio"

# Создаем пользователя и группу в контейнере с ID=1000. По умолчанию, используется идентификатор 1000,
# который совпадает с первым пользователем и группой на Linux хосте.
ARG UID=1000
ARG GID=1000
ENV UID=${UID}
ENV GID=${GID}

RUN groupadd --gid $GID game && useradd --uid $UID --gid game --shell /bin/bash --create-home game

RUN apt update -yq && apt install -y --no-install-recommends \
    gdb \
    valgrind \
    openssh-server \
    rsync \
    gdbserver \
    mc \
    strace \
    ltrace \
    linux-perf \
    python3 \
    python3-pip \
    pipx \
    cmake \
    sudo

# Для новых диструбутивов Linux разрешаем ломать всю систему питоном
RUN python3 -m pip config set global.break-system-packages true && pip install conan

# Более безопасный вариант установки conan
#RUN pipx ensurepath && pipx install conan

# Создаем default профиль для conan с поддержкой C++23
ADD ./conan/default /home/game/.conan2/profiles/
# Копируем в контейнер список зависимостей для сборки 
RUN mkdir -p /home/game/asio_game/conan
ADD ./conan/conanfile.py /home/game/asio_game/conan
RUN chown -R game:game /home/game

USER game

# Собираем отсутвующие зависимости в контейнере, будет сгенерирован кеш с артефактами сборок
RUN cd /home/game/asio_game/conan && \
    conan install . --build=missing -s build_type=Release && \
    conan install . --build=missing -s build_type=Debug


#RUN cd /app/build && \
#    cmake -DCMAKE_RUN_FROM_DOCKER_FILE=1 -DCMAKE_BUILD_TYPE=Release .. && \
#    -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake && \
#    cmake --build . --parallel

# Так работает генерация
#cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../conan_toolchain.cmake ..

#cmake -DCMAKE_RUN_FROM_DOCKER_FILE=1 -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../conan_toolchain.cmake .. && \
#    cmake --build . --parallel

# Второй контейнер в том же докерфайле
#FROM ubuntu:24.04 as run

# Создадим пользователя www
#RUN groupadd -r www && useradd -r -g www www
#USER www

# Скопируем приложение со сборочного контейнера в директорию /app.
# Не забываем также папку data, она пригодится.
#COPY --from=build /app/build/bin/game_server /app/
#COPY ./data /app/data
#COPY ./static /app/static

# Запускаем игровой сервер
#ENTRYPOINT ["/app/game_server", "/app/data/config.json", "/app/static"]
