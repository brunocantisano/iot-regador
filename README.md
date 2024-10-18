This project was bootstrapped with [Create React App](https://github.com/facebook/create-react-app).

[![Build Arduino](https://github.com/brunocantisano/iot-regador/actions/workflows/arduino.yml/badge.svg)](https://github.com/brunocantisano/iot-regador/actions/workflows/arduino.yml)

## Pré Requisitos

1. Make

```bash
sudo apt install -y build-essential
```

2. Git secrets (para evitar de subir chaves de serviços dentro do código em repositórios públicos)

```bash
git clone https://github.com/awslabs/git-secrets.git && \
    cd git-secrets/ && \
    make install
```
