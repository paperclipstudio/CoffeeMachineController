

# State machine layout

```mermaid

graph 
LR
Off(No Power) -->|Power on|low
low(Low power state) -->|Turn on|waterStart
waterStart -->|Turn off| low
waterStart -->|Select Steam| steam
steam(Steam heating <br> same as Watering but different target tempurtures) -->|Select water| waterStart
steam -->|Turn off|low
pump(Water Pump) -->|Target weight reached| waterStart
waterStart-->|Pump water|pump

subgraph <h><b>WaterHeating</b></title>
waterStart(Start) -->|no Input| read(Read Temputure)
read -->|Below target temp| max(Heater 1 and 2 on)
read -->|Above target below max| mid(Heater 1 on, heater 2 off)
read -->|Above max| off(Heater 1 and 2 off)
max --> waterStart
mid --> waterStart
off -->waterStart
end



```

