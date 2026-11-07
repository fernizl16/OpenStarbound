# StarPad — mando nativo para Starbound (sobre OpenStarbound)

Fase 1: movimiento, intercambio de sticks, sensibilidad, puntería por stick con
modos Auto y Precisión, y botones de juego. La navegación por widgets de las
interfaces llega en la fase 3.

## Cómo obtener el ejecutable (Windows)

1. Haz un fork de https://github.com/OpenStarbound/OpenStarbound en tu cuenta de GitHub.
2. Aplica el parche `starpad-fase1.patch` sobre la rama `main` de tu fork:
   `git checkout -b starpad && git am starpad-fase1.patch && git push origin starpad`
3. En la pestaña **Actions** de tu fork se compila solo. Descarga el artefacto
   `OpenStarbound-Windows-Client` (o el instalador) cuando termine.
4. Copia `packed.pak` de tu Starbound de Steam a la carpeta `assets` de OpenStarbound,
   y tu carpeta `storage` si quieres conservar personajes y opciones.
5. Conecta el mando y arranca `starbound.exe` de OpenStarbound.

## Controles (fase 1)

| Botón | Acción |
| --- | --- |
| Stick de movimiento (izquierdo por defecto) | Andar; arriba/abajo para escaleras; abajo + A baja de plataformas |
| Stick de puntería (derecho por defecto) | Dirección de puntería. Sin tocarlo, apunta hacia donde miras |
| R3 (toque) | Alternar modo Auto / Precisión |
| RT / LT | Mano principal / mano secundaria |
| A | Saltar |
| X | Interactuar |
| B | Técnica especial (F) / cerrar ventana si hay una abierta |
| Y | Inventario (abrir y cerrar) |
| LB / RB | Objeto anterior / siguiente de la barra |
| L3 | Alternar andar / correr |
| View | Ventana de fabricación |
| Start | Menú de pausa |

El ratón y el teclado siguen funcionando: mover el ratón devuelve el control al
ratón y tocar el mando lo recupera.

**Modo Auto**: con un arma dispara en la dirección del stick con imán suave
hacia enemigos cercanos; con el manipulador o un pico marca solo el primer
bloque en esa dirección; con bloques u objetos coloca en la primera casilla
libre pegada a algo sólido. **Modo Precisión**: el stick mueve un cursor libre.

## Opciones

Se guardan en `storage/starbound.config`, en la clave `starpad` (se crea con los
valores por defecto la primera vez). El menú de opciones dentro del juego llega
en la fase 5.

```json
"starpad" : {
  "enabled" : true,
  "swapSticks" : false,
  "moveStick" : { "deadzone" : 0.12, "sensitivity" : 1.0, "curve" : "progressive" },
  "aimStick"  : { "deadzone" : 0.12, "sensitivity" : 1.0, "curve" : "progressive" },
  "aimAssist" : "soft",
  "autoAimRange" : 10.0,
  "precisionSpeed" : 22.0,
  "triggerThreshold" : 0.35
}
```

| Clave | Rango | Qué hace |
| --- | --- | --- |
| `swapSticks` | true / false | `true` = moverse con el stick derecho y apuntar con el izquierdo |
| `*.sensitivity` | 0.5 – 2.0 | Sensibilidad de cada stick. En movimiento, >1 llega antes a velocidad máxima; en puntería acelera el cursor de Precisión |
| `*.deadzone` | 0 – 0.3 | Zona muerta radial |
| `*.curve` | `linear` / `progressive` | Progresiva = más fino cerca del centro |
| `aimAssist` | `off` / `soft` / `strong` | Imán de puntería hacia enemigos |
| `autoAimRange` | 3 – 30 | Distancia (casillas) del punto de mira en combate |
| `precisionSpeed` | 4 – 80 | Casillas por segundo del cursor de Precisión a tope |
| `triggerThreshold` | 0.05 – 0.95 | Cuánto hay que apretar RT/LT |

## Código

- `source/frontend/gamepad/StarGamepadLogic.hpp` — lógica pura (sticks, curvas, intercambio, selección de bloques, imán).
- `source/frontend/gamepad/StarGamepadController.*` — capa que lee el mando y maneja al jugador.
- `source/frontend/StarMainInterface.*` — enganche: el jugador apunta a la posición del mando.
- `source/client/StarClientApplication.*` — enganche: eventos y actualización por frame.
- `source/test/gamepad_test.cpp` — 14 pruebas de la lógica.
