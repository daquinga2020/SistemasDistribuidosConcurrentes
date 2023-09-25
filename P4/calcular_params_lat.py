import os
import numpy as np

# Obtener la lista de archivos de texto de la carpeta
files = os.listdir('.')

# Crear una lista vacía para almacenar los valores numéricos
values = []
temp_values = []
n = 0
# Recorrer la lista de archivos
for file in files:
    # Abrir el archivo de texto y leer los valores numéricos en una lista temporal
    with open(file, 'r') as f:
        for line in f:
            
            # Saltar a la siguiente iteración si la línea es una cadena vacía
            if line.strip() == '':
                continue
            # Convertir la línea a un número flotante y añadirlo a la lista temporal
            if float(line.strip()) < 0:
                temp_values.append(-float(line.strip()))
            else:
                temp_values.append(float(line.strip()))
    # Añadir los valores a la lista principal de valores
    values.extend(temp_values)
    #print (n)

# Convertir la lista de valores en un array de numpy
values_array = np.array(values)
# Calcular el valor máximo, mínimo, media y desviación estándar
max_value = np.max(values_array)
min_value = np.min(values_array)
mean_value = np.mean(values_array)
std_value = np.std(values_array)

# Imprimir los resultados
print(min_value,max_value,mean_value,std_value)
print("Valor máximo:", max_value)
print("Valor mínimo:", min_value)
print("Valor media:", mean_value)
print("Desviación estándar:", std_value)

