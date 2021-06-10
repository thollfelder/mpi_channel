#include <stdio.h>
#include <malloc.h>

// Returns f(x) if x is valid otherwise 0
float f_x(float *f, int len, int x)
{
    if ((x < 0) || (x >= len))
        return 0;
    else
        return f[x];
}

/**
 * @brief  Calculates the cross correlation between two discrete frunctions given as values and their length
 * 
 * @param   f                     Array of values of f(x) 
 * @param   g                     Array of values of g(x)
 * @param   len_f                 Lengtht of f
 * @param   len_g                 Length of g
 * @param   print                 Print flag
 *  
 * @return  Returns pointer to calculated (f ∘ g) (-xi) starting from xi = -len_g to xi = len_f. Smaller or bigger xi yield 0 since if both discrete
 * functions have no intersection (f ∘ g) is 0. Also prints the result.
 */
float *cross_correlation(float *f, float *g, int len_f, int len_g, int print)
{
    if (print)
    {
        printf("Calculating cross correlation of the following two functions:\n");

        printf("f(x):\n");
        for (int i = 0; i < len_f; i++)
        {
            if ((len_f - i) == 1)
            {
                printf("f(%d) = %.2f\n", i, f_x(f, len_f, i));
                continue;
            }
            printf("f(%d) = %.2f, ", i, f_x(f, len_f, i));
        }

        printf("g(x):\n");
        for (int i = 0; i < len_g; i++)
        {
            if ((len_g - i) == 1)
            {
                printf("g(%d) = %.2f\n", i, f_x(g, len_g, i));
                continue;
            }
            printf("g(%d) = %.2f, ", i, f_x(g, len_g, i));
        }

        printf("Solution:\n");
    }

    float *solutions = malloc(sizeof(float) * (len_f + len_g + 1));

    // Shift g(x) past f(x) from -len_g to len_f
    for (int xi = -len_g; xi <= len_f; xi++)
    {
        // Used to store result of every integral with fixed xi
        float f_x_g_xi = 0;

        // Calculate integral from 0 to len_f is enough since on every other position it must be zero
        for (int x = 0; x < len_f; x++)
        {
            f_x_g_xi += f_x(f, len_f, x) * f_x(g, len_g, x - xi);
        }

        solutions[xi + len_g] = f_x_g_xi;

        if (print)
            printf("(f ∘ g) (%d) = %.3f, ", xi, f_x_g_xi);
    }
    printf("\n");

    return solutions;
}

/**
 * @brief Finds the number of all maxima of a given cross correlation. Maxima is defined as f(x) where f(x-1) < f(x) > f(x+1) holds.
 * 
 * @param   f                     Array of values of f(x) 
 * @param   g                     Array of values of g(x)
 * @param   len_f                 Lengtht of f
 * @param   len_g                 Length of g
 *  
 * @return  Returns the number of maxima found. If no maxima is found it returns 0.
 */
int cross_correlation_maxima(float *f, float *g, int len_f, int len_g)
{
    // First calculate the cross correlation of (f ∘ g)
    float *solutions = cross_correlation(f, g, len_f, len_g, 0);

    // Init maxima count with 0
    int count_max = 0;

    // Calculate all maxima
    for (int i = 1; i < (len_f + len_g); i++)
    {
        if ((solutions[i - 1] < solutions[i]) && (solutions[i] > solutions[i + 1]))
            count_max++;
    }

    printf("Number of maxima: %d\n", count_max);
    free(solutions);
    return count_max;
}

/**
 * @brief Finds the first maximum of a given cross correlation. Maxima is defined as f(x) where f(x-1) < f(x) > f(x+1) holds.
 * 
 * @param   f                     Array of values of f(x) 
 * @param   g                     Array of values of g(x)
 * @param   len_f                 Lengtht of f
 * @param   len_g                 Length of g
 *  
 * @return  Prints the value of the first maxium and the corresponding xi. If no maxium is found it prints a matching string
 */
void cross_correlation_first_maximum(float *f, float *g, int len_f, int len_g)
{
    float *solutions = cross_correlation(f, g, len_f, len_g, 0);

    // Calculate first
    for (int xi = -(len_g + 1); xi < len_f; xi++)
    {
        if ((solutions[xi + len_g - 1] < solutions[xi + len_g]) && (solutions[xi + len_g] > solutions[xi + len_g + 1]))
        {
            printf("First maximum is (f ∘ g) (%d) = %.3f\n", xi, solutions[xi + len_g]);
            free(solutions);
            return;
        }
    }
    printf("No maximum found\n");
    free(solutions);
    return;
}

int main()
{

    float f[] = {1.f, 0.25f, 0.1f, 4.f, 0.f, 0.3f, 2.f};
    int len_f = 7;
    float g[] = {0.5, 0.2};
    int len_g = 2;

    cross_correlation(f, g, len_f, len_g, 1);
    cross_correlation_maxima(f, g, len_f, len_g);
    cross_correlation_first_maximum(f, g, len_f, len_g);

    return 0;
}
