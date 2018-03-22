# Grunt #

Aplicação Gateway da plataforma Qogni.

https://qogni33.atlassian.net/wiki/spaces/OVSHW/pages/1867804/Arquitetura+Grunt+Gateway+WSN

## Competências

* Receber comandos da plataforma Overseer(Subscribe)
* Receber configurações de dispositivos de campo da plataforma Overseer(Subscribe)
* Enviar dados para plataforma Overseer(Publish)
* Enviar notificações de Adição/Remoção/Status de dispositivos de campo para plataforma Overseer(Publish)
* Enviar notificações de status e auto diagnóstico para plataforma Overseer(Publish)
* Receber notificações de Alarmes da plataforma Overseer(Subscribe)
* Registro local de mensagens a serem enviadas(Store and Forward)

## Características da Arquitetura

* Modularidade para conectividade (IO stream)
* Modularidade para acesso a dispositivos de campo(IOB, WSN, Modbus TCP) abstraindo parâmetros de configuração
* Parametrização de tamanho de filas e buffers para otimizar recursos
* Modularidade para comportamento Low Power


