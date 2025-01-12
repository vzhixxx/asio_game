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
    sudo \
    procps \
    net-tools \
    locales \
    postgresql postgresql-contrib

# Для новых диструбутивов Linux разрешаем ломать всю систему питоном
RUN python3 -m pip config set global.break-system-packages true && pip install conan

# Более безопасный вариант установки conan
#RUN pipx ensurepath && pipx install conan

# Создаем default профиль для conan с поддержкой C++23
ADD ./conan/default /home/game/.conan2/profiles/
# Создаем каталоги для сборки и работы приложения
RUN mkdir -p /home/game/asio_game/conan && \
    mkdir -p /opt/asio_game/bin && \
    mkdir -p /opt/asio_game/static && \
    mkdir -p /opt/asio_game/data

# Копируем в контейнер список зависимостей для сборки 
ADD ./conan/conanfile.py /home/game/asio_game/conan

RUN chown -R game:game /home/game && \
    chown -R game:game /opt/asio_game/*
# Задаем пароль пользователя и добавляем пользователя в группу sudo
RUN echo 'game:123456' | chpasswd && usermod -aG sudo game

# Меняем пользователя в контейнере
USER game

# Собираем отсутвующие зависимости в контейнере, будет сгенерирован кеш с артефактами сборок
RUN cd /home/game/asio_game/conan && \
    conan install . --build=missing -s build_type=Release && \
    conan install . --build=missing -s build_type=Debug

# Копируем в контейнер статические файлы фронтенда и конфига
ADD ./static /opt/asio_game/static
ADD ./data /opt/asio_game/data

# Запускаем игровой сервер
#ENTRYPOINT ["/app/game_server", "/app/data/config.json", "/app/static"]
