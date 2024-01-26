#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

void removeSpaces(char* str)
{
    int count = 0; // 공백이 아닌 문자의 수

    for (int i = 0; str[i]; i++)
    {
        if (str[i] != ' ') // 띄어쓰기가 아니라면
            str[count++] = str[i]; // 해당 문자를 저장
    }

    str[count] = '\0'; // 문자열 끝
}

int main(void)
{
    while (1)
    {
        char input[100] = {'0', };

        printf("Input: ");
        fgets(input, sizeof(input), stdin);

        if (input[strlen(input) - 1] == '\n')
            input[strlen(input) - 1] = '\0';

        if (input[0] == '\0')
            break;

        removeSpaces(input); // 입력된 문자열에서 띄어쓰기 제거

        char str1[100] = { '0', };
        char str2[100] = { '0', };
        int boole = 0;
        int pt = 0;

        for (int i = 0; i < strlen(input); i++)
        {
            if (input[i] == '+')
            {
                if (boole != 0)
                {
                    boole = 3;
                    break;
                }

                boole = 1;
                pt = i + 1;
                continue;
            }
            else if (input[i] == '-')
            {
                if (boole != 0)
                {
                    boole = 3;
                    break;
                }

                boole = 2;
                pt = i + 1;
                continue;
            }
            else if (input[i] < '0' || input[i] > '9')
            {
                boole = 3;
                break;
            }

            if (boole == 0)
            {
                str1[i] = input[i];
            }
            else
            {
                str2[i - pt] = input[i];
            }
        }

        long result;
        char *resultStr; // 문자열 포인터를 저장할 변수 추가

        switch (boole)
        {
            case 0:
                resultStr = (char *) syscall(450, str1); // 결과를 문자열 포인터로 받아옵니다.
                printf("Output: %s\n", resultStr); // 문자열을 출력
                break;
            case 1:
                result = syscall(452, atol(str1), atol(str2));
                printf("Output: %ld\n", result);
                break;
            case 2:
                result = syscall(451, atol(str1), atol(str2));
                printf("Output: %ld\n", result);
                break;
            default:
                printf("Wrong Input!\n");
                break;
        }
    }

    return 0;
}


