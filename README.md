# Implementación de Internet de las Cosas | Trabajo Práctico:

*Autor*: Cristian Romero<br>
*Año*: 2026

---
## Desafíos:

### Desafío 1: Led RGB
---

**Descripción:** Al iniciar la aplicación el led RGB inicia en rojo para indicar que se está intentando conectar a la red. Una vez que se coneta a la red Wi-Fi, parpadea cada 5 segundos con cada POST.

**Estado:** Listo &#9989;

---
### Desafío 2: Pressure

---

**Descripción:** Enviar el dato de presión. El código de ejemplo envía temperatura, humedad y presión. Muy posiblemente se haya usado el bme280 y actualmente solo se cuenta con el bmp280 que envía datos de temperatura y presión, no envía humedad.

El código actual era:
```
void bmp280_read(bmp280_t *dev, float *pressure, float *temperature, float *humidity)
```
Ahora quedó de la siguiente manera:

```
void bmp280_read(bmp280_t *dev,float *temperature,float *pressure)
```

**Estado:** Listo &#9989;

---

### Desafío 3: fix_key

---
**Descripción:** corregir que el dispositivo no está enviando la key.

El POST_DATA_TEMPLATE:

```
static char *POST_DATA_TEMPLATE = "id="DEVICE_ID"&key="DEVICE_KEY"&t=%0.2f&p=%0.2f";  // Formato URL encoded
```

**Estado:** Listo &#9989;

---
### Desafío 4: Timestamp

---

**Descripción:** se agregó una marca de tiempo a la comunicación entre el back end y el dispositivo.

```
app.post('/measurement', async function (req, res) {
    console.log("device id    : " + req.body.id + " key         : " + req.body.key + " temperature : " + req.body.t + " pressure    : " + req.body.p);	
    
    // Agregamos Timestamp
    const insertedId = await insertMeasurement({
        id:req.body.id,
        t:req.body.t,
        h:req.body.p,
        timestamp: new Date()    // Campo con marca de tiempo en formato ISO 8601.
    });

	res.send("received measurement into " +  insertedId);
});
```

Nota: lo ideal sería trabajar con un RTC y enviar la marca de tiempo a la hora que se realizó la medición. Esta marca de tiempo solo representa hora de comunicación.

**Estado:** Listo &#9989;

---

### Desafío 5:

---

**Descripción:** 

```

```


**Estado:** No terminado aún &#128736;&#65039;;

---
