Short description in English: this project makes a data-source plugin for great [AmiBroker](https://www.amibroker.com/) software, that obtains quotes data in real-time from QUIK terminal using [t18qsrv](https://github.com/Arech/t18qsrv) proxy-server plugin for QUIK. QUIK terminal is commonly used to trade Russian stock/futures/etc market. You probably have to understand Russian in order do what the QUIK is used for, so I don't see much sence in making a full translation of the project description. However, note that the code comments are mostly written in English.

# Плагин AmiBroker для получения данных из QUIK по сети

Терминал [AmiBroker](https://www.amibroker.com/) на сегодняшний день является, вероятно, одним лучших средств (лучшим средством?) для работы с финансовыми таймсериями. Благодаря наличию системы плагинов, в AmiBroker можно в режиме реального времени загружать данные из любых источников.

Терминал QUIK, широко используемый на Российском рынке, в своей поставке содержит плагин для передачи данных в AmiBroker, однако этот плагин требует, чтобы и QUIK и AmiBroker работали бы одновременно на одной машине. Это не всегда удобно и даже возможно из-за различных проблем разрядности, совместимости и т.д.

Проект `Q2Ami` с помощью работающего внутри QUIK прокси [t18qsrv](https://github.com/Arech/t18qsrv)  даёт возможность разнести по разным машинам работающий с серверами брокера экземпляр QUIK и терминал AmiBroker, в который по сети будут передаваться котировки. Другими словами, связка `Q2Ami`+`t18qsrv` позволяет передавать на другую машину и выводить в AmiBroker поток обезличенных сделок.

Код плагина устроен таким образом, что позволяет "на лету" выполнять преобразования потока сделок в любой нужный вид (вам придётся написать нужный код для этого) и выводить результат в отдельный тикер AmiBroker. В качестве примера см. реализацию класса `::t18::_Q2Ami::modes::ticks` в файле `q2ami_convs.h`.

## Статус кода

В целом, можно охарактеризовать скорее как "очень стабильная бета", т.к. в основном всё работает правильно, но есть два нюанса:

1. в некоторых случаях (всегда?) на фьючерсах первичное получение данных от начала текущей торговой сессии сопровождается срабатываем уведомления от кода контроля последовательности времени котировок (собственного для `Q2Ami`) из-за незначительного (обычно несколько секунд начала вечерней сессии) "наслоения"/перезаписи данных. Причину такого поведения определить пока не удалось, как не удалось даже понять, возникает ли проблема из-за ошибки в `Q2Ami`, или всё-таки из-за ошибки/недокументированного поведения AmiBroker. К сожалению, SDK AmiBroker содержит исключительно бедные и неполные описания важных компонентов и функций терминала и их связь с плагинами, а спецификация последовательности и условий вызова некоторых функций плагина вовсе отстуствует, поэтому понять, что происходит, в некоторых случаях, трудно. Кроме того, в AmiBroker встроен код, препятствующий отладке терминала (и, следовательно, любого плагина внутри терминала) обычным отладчиком, что сильно затрудняет разбор ошибок вообще.
  В целом, ошибка пока не рассматривается как очень серьёзная мешающая работе, и требующая немедленного исправления. Если вы найдёте причину и решение - обязательно сделайте pull request!

2. отсутствует код восстановления соединения с прокси `t18qsrv` при потере связи. В случае потери соединения необходимо перезапустить AmiBroker (обычно это не большая и очень редкая проблема, поскольку в силу незащищённости канала передачи данных между `Q2Ami` и `t18qsrv`, решение имеет смысл использовать лишь внутри одной защищённой локальной сети, где разрывы соединений - крайне редкое, если вообще не вероятное, явление).


### Условия использования

Программа поставляется на условиях стандартной лицензии свободного ПО GNU GPLv3. Если вы хотите использовать плагин или его часть так, как лицензия прямо не разрешает, пишите на `aradvert@gmail.com`, договоримся.

Если вы зарабатываете с участием плагина деньги, то, наверняка, должны понимать сколько труда стоит создать такую работу. Сделайте донат для покрытия расходов на разработку на своё усмотрение. За деталями так же пишите на `aradvert@gmail.com`.

#### Дополнительная информация

Код содержит некоторые файлы из AmiBroker SDK, разрешённые к распространению. Эти файлы помечены заголовком:
```c++
///////////////////////////////////////////////////////////////////////
// Copyright (c) 2001-2010 AmiBroker.com. All rights reserved. 
//
// Users and possessors of this source code are hereby granted a nonexclusive, 
// royalty-free copyright license to use this code in individual and commercial software.
//
// AMIBROKER.COM MAKES NO REPRESENTATION ABOUT THE SUITABILITY OF THIS SOURCE CODE FOR ANY PURPOSE. 
// IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND. 
// AMIBROKER.COM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOURCE CODE, 
// INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. 
// IN NO EVENT SHALL AMIBROKER.COM BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL, OR 
// CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, 
// WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, 
// ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOURCE CODE.
// 
// Any use of this source code must include the above notice, 
// in the user documentation and internal comments to the code.
///////////////////////////////////////////////////////////////////////
```


### Поддерживаемый компилятор

`Q2Ami` разработан с использованием превосходного компилятора [Clang](https://clang.llvm.org/) v6 набора LLVM, поэтому версия 6 или более новые версии Сlang будут работать сразу. Другие современные компиляторы, полноценно поддерживающие спецификацию С++17 теоретически должны работать тоже, но, возможно, придётся внести небольшие правки.

Среда Visual Studio 2015 (проект которой присутствует в исходных кодах) используется только как редактор кода и инструмент управления компиляцией. Использование её не обязательно. Для компилятора VC версии 2015 проект, скорее всего, будет не по зубам, из-за зависимостей на фреймфорк [t18](https://github.com/Arech/t18), требующий поддержки С++17, но, возможно, более свежие VC2017 или VC2019 уже справятся.

Обратите внимание, что в проекте определён `Post-Build Event` в котором собранная `.dll` плагина копируется в папку плагинов AmiBroker. Проверьте и перенастройте под себя, или уберите этот шаг.

### Внешние зависимости

- Кроме стандартных компонентов STL (использовалась версия stl из VC2015, но с другими не более старыми версиями проблем быть не должно)
 используются только некоторые компоненты библиотеки [Boost](https://www.boost.org/) в режиме "только заголовочные файлы" (использовалась версия 1.70, более свежие должны работать из коробки). Компиляции Boost не требуется.

- Фреймворк [t18](https://github.com/Arech/t18) содержит необходимые описания протокола и некоторые иные нужные части. Фреймворк достаточно скачать/клонировать в __над__-каталог проекта, в папку `../t18`, и он подхватится автоматически.

- Быстрая lock-free очередь [readerwriterqueue](https://github.com/cameron314/readerwriterqueue). Проект надо скачать/клонировать в __над__-каталог `../_extern/readerwriterqueue/` и всё подхватится автоматически.

- [Заголовочная версия INIReader](https://github.com/jtilly/inih) должна находиться в __над__-каталоге `../_extern/inih/`

- Система логирования [spdlog](https://github.com/gabime/spdlog). Если скачать/склонировать в __над__-каталог `../_extern/spdlog-1.3.1/`, подхватится автоматически. При неизменности интерфейса, должно быть, будут работать и более новые версии, однако в случае ошибок - берите версию [1.3.1](https://github.com/gabime/spdlog/releases/tag/v1.3.1). Путь к заголовочным файлам прописывается на листе `VC++ Directories` свойств проекта в атрибуте `Include Directories`.

## Использование

QUIK с работающим внутри него прокси [t18qsrv](https://github.com/Arech/t18qsrv) должен быть запущен и доступен по сети.

Проверить, видит ли AmiBroker установленный плагин, можно в меню `Tools / Plugins...`. Там должен быть пункт `My QUIK2Ami`, столбец `type` должен иметь значение `data`.

Под данные из QUIK придётся создать отдельную базу данных (это общая рекомендация AmiBroker, из-за его внутреннего устройства не смешивать обычные базы с большими историческими данными с базами внешних data-source плагинов). Для этого:

1. Сначала желательно создать в любом месте на диске (куда может писать AmiBroker) пустую папку, в которую сразу следует поместить конфиг-файл `cfg.txt`, определяющий в числе прочего IP-адрес и порт, на котором работает `t18qsrv`. Настройки конфига описаны дальше в отдельном разделе, к которому рекомендую перейти после знакомства со следующими шагами.

2. Далее выбираем меню `File / New / Database...`, в появившемся диалоге в разделе "Database folder" указываем путь к ранее созданной папке, в разделе "Base time interval" выбираем `Tick` и нажимаем `Create`. Далее станут доступны опции "Data source", где надо выбрать `My QUIK2Ami` и задать достаточно большое число баров в "Number of bars". Переключатель "Local data storage" лучше оставлять в положении `Enable`, иначе данные (обезличенные сделки) по прошлым торговым сессиям (если их не сохраняет ваш брокер, а скорее всего он их не сохраняет), пропадут.

    - Если вы реализуете дополнительные режимы обработки/агрегирования обезличенных сделок (например, будете сразу конвертировать их в таймфрейм М1), рекомендую оставлять переключатель "Base time interval" в прежнем значении `Tick`. Это гарантирует, что AmiBroker не будет никак пытаться дополнительно конвертировать получаемые от плагина данные перед их сохранением в свою базу данных.

    - Важно понимать, что в отличие от исторических баз (тип Data Source `(local database)`), где параметр "Number of bars" не влияет на объём возможной истории по инструменту, для плагинов "Number of bars" глубину доступной истории задаёт однозначно и она не может быть превышена, - старые данные просто исчезают (перезаписываются). Выбирайте значение этого параметра исходя из собственных потребностей и мощности компьютера (учтите, что при использовании data-source плагинов AmiBroker и его afl-движок становится несколько менее эффективным). В качестве стартовой точки, должно быть, вполне пойдёт значение в 150000 баров. Для самых ликвидных инструментов фондовой секции МосБиржи этого значения обычно хватает, чтобы удержать все сделки двух-трёх-четырёх торговых дней. Для менее ликвидных - недели и более. Для самых ликвидных фьючерсов же может не хватить даже на день.

3. Далее, если уведомление статуса плагина (в правом нижнем углу окна в статус-баре левее от объёма доступной памяти) показывает, что плагин подсоединился к `t18qsrv` (настроили правильный IP-адрес в конфиге?), то нужно нажать кнопку `Configure`. Тогда плагин свяжется с сервером `t18qsrv`, получит свойства заданных в конфиг-файле `cfg.txt` инструментов, и добавит их в список тикеров AmiBroker. После этого можно закрывать диалог через "ОК", выбирать нужный тикер из списка на вкладке `Symbols` и работать.

    - Нажимать `Configure` в этом диалоге (доступном так же через меню `File / Database settings`) потребуется так же каждый раз при добавлении новых инструментов в конфиг `cfg.txt` и перезапуска процесса терминала, поскольку AmiBroker не предоставляет иного доступа к API добавления тикеров, кроме как через обработку события конфигурирования.

    - Удалённые из `cfg.txt` инструменты автоматически удаляться ни в каком случае не будут. При необходимости удалите их вручную через вкладку `Symbols` (выбрать набор тикеров с помощью зажатой ctrl, затем в меню правой кнопки `Delete`).

    - Никогда не изменяйте назначенные плагином имена тикеров, иначе всё сломаете.

### Об именовании тикеров, принятом в Q2Ami

Внутри AmiBroker название тикера (то, что показывается на вкладке `Symbols`) является его уникальным идентификатором, поэтому для того, чтобы обеспечить возможность адресовать уникальную комбинацию из режима обработки потока обезличенных сделок для конкретного инструмента конкретной торговой секции биржи, необходимо уместить все эти три признака в один строковый идентификатор. В `Q2Ami` принят следующий шаблон именований тикеров:

    <название режима>|<id режима>@<код инструмента>@<код класса инструмента>

В опубликованной версии `Q2Ami` есть только один режим обработки потока обезличенных сделок, - он же "никакой обработки", - который называется `ticks` (его реализация описана в классе `::t18::_Q2Ami::modes::ticks` файла `q2ami_convs.h` и может быть использована как шаблон для реализации более сложных алгоритмов). Соответственно, `<название режима>|<id режима>` для всех инструментов с этим режимом будет начинаться с текста `ticks|0` (`<id режима>` это просто уникальный численный идентификатор режима, назначаемый автоматически; он нужен для упрощения обращения к коду, который реализует этот режим).

Таким образом, например:
- потоку обезличенных сделок по акциям Газпрома (код `GAZP`) фондовой секции МосБиржи (код класса `TQBR`) будет соответствовать тикер в Ami `ticks|0@GAZP@TQBR`
- обезличенным сделкам фьючерса (код класса `SPBFUT`) на нефть, истекающего в ноябре 2019 (код `BRX9`), - тикер `ticks|0@BRX9@SPBFUT`

### Конфигурирование Q2Ami. Опции cfg.txt

Вся настройка плагина выполняется посредством конфиг-файла `cfg.txt`, который должен быть расположен в директории текущей базы данных AmiBroker. Если при создании новой базы данных в её папке отсутсутствует конфиг-файл, то он будет создан автоматически со следующим шаблонным содержанием:

```ini
# default config, edit as necessary

# server's ip&port address:
serverIp = 111.222.113.224
serverPort = 8945

# specify category of tickers to fetch using classCode as [section name]
# On MOEX.com the TQBR code is used for the stock market section and the SPBFUT for the derivatives market
# QJSIM is used in a QUIK Junior (QUIK's demo) program to address simulated data for stock market
[TQBR]

# tickers is a comma separated list of tickers codes for the class
tickers = GAZP,SBER

# sessionStart and sessionEnd variables serves as default values for ticker time filters.
# Each may be overridden with corresponding <ticker>_sessionStart and <ticker>_sessionEnd.
# set to -1 to disable filtering
sessionStart = 100000
sessionEnd = 184000

# defModes can be overridden for each ticker with <ticker>_modes
defModes = ticks

# ExpDailyDealsCount is a daily expected number of deals for a ticker
defExpDailyDealsCount = 50000

```

Первые два параметра (`serverIp` и `serverPort`) задают сетевую адресацию машины, где искать сервер `t18qsrv`. Если вы не меняли номер порта при сборке `t18qsrv`, то вам потребуется задать только правильный IP-адрес.

Все остальные параметры описывают, какие инструменты надо вытягивать из QUIK, как их фильтровать, и с какими режимами обработки потока обезличенных сделок их надо выводить в AmiBroker. Для этого конфиг файл разбивается на секции (описываются `[`квадратными `]` скобками), название каждой из которых описывает к какому классу относятся заданные в секции инструменты. В примере выше определена только одна секция `[TQBR]`, которая соответствует фондовому рынку МосБиржи. Код `SPBFUT` описывал бы срочный рынок МосБиржи. Название этих строк - `TQBR` или `SPBFUT` - соответствуют тому, как это определено в QUIK, поэтому изменить их невозможно.

Внутри каждой секции возможны следующие параметры:

- `tickers` задаёт список кодов инструментов внутри текущего класса, разделённый запятыми.
- Два параметра `sessionStart` и `sessionEnd` могут (при значении отличном от `-1`) задавать глобальный (для каждого тикера текущего класса/секции) фильтр времени в military формате.

    - Использованные в примере значения соответственно `100000` и `184000` соответствуют 10:00:00 утра и 18:40:00 вечера, т.е. основной торговой сессии фондового рынка. Соответственно, поскольку сделки аукциона открытия и аукциона закрытия в это время не попадают, то в AmiBroker они не попадут.

    - Для каждого тикера можно задать индивидуальные значения этих параметров используя шаблон `<ticker>_sessionStart` и `<ticker>_sessionEnd`. Например, чтобы отключить всю фильтрацию по времени для сделок по акциям Газпрома, добавляем строки
```ini
GAZP_sessionStart = -1
GAZP_sessionEnd = -1
```

- параметр `defModes` задаёт разделённый запятыми глобальный список режимов обработки/агрегирования потока обезличенных сделок. Например, если вы реализуете конвертер в таймфрейм М1 и назовёте его `M1`, то этому параметру можно задать значение `ticks,M1` для экспорта в AmiBroker и чистых тиков, и таймфрейма М1.

    - совершенно аналогично для каждого тикера можно переопределить его список режимов пользуясь параметром, название которого собрано по шаблону `<ticker>_modes`

- `defExpDailyDealsCount`: поскольку AmiBroker обновляет в локальной базе только те тикеры, с которыми пользователь в данный момент работает (строит графики, например), а поток обезличенных сделок приходит непрерывно, то все полученные сделки необходимо кешировать в памяти, чтобы иметь возможно быстро вернуть их в AmiBroker при получении запроса. Параметр `defExpDailyDealsCount` просто задаёт начальный размер `::std::vector`, который накапливает пришедшие сделки. Короче, это просто настройка величины пре-аллоцирования памяти для того, чтобы в процессе работы не фрагментировалась лишний раз память и не тратились ресурсы на реаллокацию и копирование данных. Особо над ней заморачиваться нет смысла, т.к. видимого ущерба производительности, скорее всего, даже самое неудачное малое значение не нанесёт. Значение немного большее среднего числа сделок за день подойдёт хорошо.

    - Для переопределения значения для конкретного тикера используйте шаблон имени параметра `<ticker>_ExpDailyDealsCount`



### Решение проблем

Начинайте с контроля логов: плагин записывает текстовые логи некоторых основных внутренних процессов в файл `logs.txt` (есть так же архивные копии `logs.N.txt`, где `N` от 1 до 3), директории текущей базы данных. Макс. размер одного файла 64Кб.

Самый подробный лог пишется в стандартный отладочный поток Windows через WinApi `::OutputDebugString()`. Используйте, например, [DebugView](https://docs.microsoft.com/en-us/sysinternals/downloads/debugview).
